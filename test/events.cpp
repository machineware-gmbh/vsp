/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

#include "vsp/dispatcher.h"
#include "vsp/payloads.h"

using namespace testing;
using namespace vsp;

class events_test : public Test
{
protected:
    static constexpr const char* HOST = "localhost";

    // the cpus run up to one quantum (1us) ahead; after a free run was
    // stopped, a step of a single quantum might not execute anything
    static constexpr u64 STEP_NS = 10000;

    // only an upper bound, sanitizer builds (e.g. TSAN in CI) are slow
    static constexpr u64 TIMEOUT_MS = 120000;

    // without DMI, every bus access goes through the traced sockets
    events_test(const vector<string>& extra = {}):
        sess(), subp(), port(launch(sess, subp, extra)) {
        write_loop();
    }

    static u16 launch(vsp::session& sess, mwr::subprocess& subp,
                      vector<string> extra) {
        vector<string> args{ "-c", "system.cpu0.data.allow_dmi=false", "-c",
                             "system.cpu1.data.allow_dmi=false" };
        args.insert(args.end(), extra.begin(), extra.end());
        return connect_simple_vp(sess, subp, args);
    }

    virtual ~events_test() {
        sess.quit();
        subp.terminate();
    };

    // endless loop at address 0, so that both cpus keep accessing the bus
    void write_loop() {
        target* targ = sess.find_target("system.cpu0");
        MWR_REPORT_ON(!targ, "target system.cpu0 not found");
        const vector<u8> inf_loop_inst{ 0x00, 0x00, 0x00, 0x20 };
        targ->write_vmem(0x0, inf_loop_inst);
    }

    vsp::module* mod(const string& name) {
        vsp::module* m = sess.find_module(name);
        MWR_REPORT_ON(!m, "module %s not found", name.c_str());
        return m;
    }

    vsp::session sess;
    mwr::subprocess subp;
    u16 port;
};

TEST_F(events_test, published_events) {
    EXPECT_TRUE(mod("system.cpu0.data")->is_traceable());
    EXPECT_THAT(mod("system.cpu0.data")->events(), ElementsAre("trace"));
    EXPECT_TRUE(mod("system.cpu0")->events().empty());
    EXPECT_TRUE(mod("system.cpu0")->is_traceable());
    EXPECT_TRUE(mod("")->is_traceable());
    EXPECT_FALSE(mod("system.cpu0")->has_leds());
    EXPECT_FALSE(mod("system.cpu0")->has_uart());
}

TEST_F(events_test, trace_tlm) {
    vsp::module* cpu = mod("system.cpu0");
    vector<pair<trace_info, trace_tlm>> events;

    cpu->on_trace<trace_tlm>([&](const trace_info& info, const trace_tlm& tx) {
        events.emplace_back(info, tx);
    });
    EXPECT_TRUE(cpu->is_traced());
    EXPECT_FALSE(mod("system.cpu1")->is_traced());

    // all events up to the stop have been delivered when step returns
    sess.step(STEP_NS, TIMEOUT_MS);
    ASSERT_FALSE(events.empty());

    bool has_fw = false, has_bw = false;
    for (const auto& [info, tx] : events) {
        EXPECT_EQ(&info.port, sess.find_module("system.cpu0.data"));
        EXPECT_TRUE(tx.is_read());
        EXPECT_EQ(tx.data.size(), 4);
        has_fw |= info.is_request();
        has_bw |= info.is_response();
        if (info.is_response()) {
            EXPECT_EQ(tx.response, "TLM_OK_RESPONSE");
        }
    }

    EXPECT_TRUE(has_fw);
    EXPECT_TRUE(has_bw);
    EXPECT_EQ(sess.events_dropped(), 0);

    // removing the last handler unsubscribes
    cpu->on_trace<trace_tlm>(nullptr);
    EXPECT_FALSE(cpu->is_traced());
    size_t count = events.size();
    sess.step(STEP_NS, TIMEOUT_MS);
    EXPECT_EQ(events.size(), count);
}

TEST_F(events_test, handlers_of_parents) {
    vsp::module* root = mod("");
    vsp::module* cpu0 = mod("system.cpu0");

    // the root traces every socket in the system, so step less
    const u64 step_ns = 2000;

    std::set<string> senders;
    size_t root_count = 0, cpu_count = 0;
    root->on_trace([&](const trace_info& info, const trace_payload& tx) {
        senders.insert(info.port.hierarchy_name());
        if (std::holds_alternative<trace_tlm>(tx))
            root_count++;
    });
    cpu0->on_trace<trace_tlm>(
        [&](const trace_info& info, const trace_tlm& tx) { cpu_count++; });

    sess.step(step_ns, TIMEOUT_MS);
    EXPECT_GT(cpu_count, 0);
    EXPECT_GE(root_count, cpu_count);
    EXPECT_TRUE(senders.count("system.cpu0.data"));
    EXPECT_TRUE(senders.count("system.cpu1.data"));

    // removing the root handler keeps the cpu subscribed
    root->on_trace(nullptr);
    EXPECT_FALSE(root->is_traced());
    EXPECT_TRUE(cpu0->is_traced());

    size_t before = cpu_count;
    root_count = 0;
    sess.step(step_ns, TIMEOUT_MS);
    EXPECT_GT(cpu_count, before);
    EXPECT_EQ(root_count, 0);
}

TEST_F(events_test, protocols_on_one_module) {
    vsp::module* cpu = mod("system.cpu0");
    size_t tlm = 0;

    cpu->on_trace<trace_tlm>(
        [&](const trace_info& info, const trace_tlm& tx) { tlm++; });
    cpu->on_trace<trace_gpio>(
        [&](const trace_info& info, const trace_gpio& tx) {});

    // removing one protocol keeps the subscription for the other
    cpu->on_trace<trace_gpio>(nullptr);
    EXPECT_TRUE(cpu->is_traced());
    sess.step(STEP_NS, TIMEOUT_MS);
    EXPECT_GT(tlm, 0);
}

TEST_F(events_test, errors) {
    vsp::module* cpu = mod("system.cpu0");

    // cpu0 has no leds and no terminal
    EXPECT_THROW(cpu->on_led([](const led_event& ev) {}), mwr::report);
    EXPECT_THROW(cpu->on_uart([](const uart_event& ev) {}), mwr::report);

    // removing a handler that was never installed is fine
    EXPECT_NO_THROW(cpu->on_trace(nullptr));
    EXPECT_NO_THROW(cpu->on_led(nullptr));

    sess.run();
    ASSERT_TRUE(sess.check_running());
    try {
        cpu->on_trace<trace_tlm>(
            [](const trace_info& info, const trace_tlm& tx) {});
        ADD_FAILURE() << "subscribing while running did not throw";
    } catch (std::exception& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("simulation running"));
    }
    EXPECT_FALSE(cpu->is_traced());
    sess.stop();
}

TEST_F(events_test, reconnect) {
    size_t count = 0;
    mod("system.cpu0")
        ->on_trace<trace_tlm>(
            [&](const trace_info& info, const trace_tlm& tx) { count++; });

    sess.run();
    mwr::usleep(10000);
    sess.disconnect();

    // handlers are kept by name and subscribed again
    sess.connect(HOST, port);
    EXPECT_TRUE(mod("system.cpu0")->is_traced());

    count = 0;
    sess.step(STEP_NS, TIMEOUT_MS);
    EXPECT_GT(count, 0);
}

TEST_F(events_test, replay) {
    vsp::module* cpu = mod("system.cpu0");
    vector<pair<string, trace_clk>> clocks;

    // new subscribers receive the current clock once, it never changes
    cpu->on_trace<trace_clk>([&](const trace_info& info, const trace_clk& tx) {
        clocks.emplace_back(info.port.hierarchy_name(), tx);
    });

    sess.step(STEP_NS, TIMEOUT_MS);
    ASSERT_EQ(clocks.size(), 1);
    EXPECT_EQ(clocks[0].first, "system.cpu0.clk");
    EXPECT_EQ(clocks[0].second.period_ns, 1);
    EXPECT_TRUE(clocks[0].second.posedge);
    EXPECT_DOUBLE_EQ(clocks[0].second.duty_cycle, 0.5);
}

TEST_F(events_test, print) {
    vector<string> lines;

    // only the data socket, other sockets replay their state first
    mod("system.cpu0.data")
        ->on_trace([&](const trace_info& info, const trace_payload& tx) {
            stringstream ss;
            ss << info << " " << tx;
            lines.push_back(ss.str());
        });

    sess.step(STEP_NS, TIMEOUT_MS);
    ASSERT_FALSE(lines.empty());
    EXPECT_THAT(lines[0], HasSubstr("system.cpu0.data >> TLM READ @0x"));
}

class generator_test : public events_test
{
protected:
    generator_test(): events_test({ "-c", "system.gen.enabled=true" }) {}
};

TEST_F(generator_test, published_events) {
    EXPECT_TRUE(mod("system.leds")->has_leds());
    EXPECT_FALSE(mod("system.leds")->has_uart());
    EXPECT_TRUE(mod("system.term0")->has_uart());
    EXPECT_FALSE(mod("system.term0")->has_leds());
    EXPECT_FALSE(mod("system.gen")->has_leds());
    EXPECT_FALSE(mod("system.gen")->has_uart());
    EXPECT_TRUE(mod("system")->has_leds());
    EXPECT_TRUE(mod("system")->has_uart());

    EXPECT_THROW(mod("system.gen")->on_uart([](const uart_event& ev) {}),
                 mwr::report);
}

TEST_F(generator_test, on_led) {
    vsp::module* leds = mod("system.leds");
    vector<led_event> events;
    leds->on_led([&](const led_event& ev) { events.push_back(ev); });

    sess.step(100000, TIMEOUT_MS);
    ASSERT_GE(events.size(), 4);

    // led <n % 4> toggles on tick <n>, all start off
    bool states[4] = {};
    u64 last_ps = 0;
    for (size_t i = 0; i < events.size(); i++) {
        const led_event& ev = events[i];
        EXPECT_EQ(&ev.leds, leds);
        EXPECT_EQ(ev.index, i % 4);
        states[ev.index] = !states[ev.index];
        EXPECT_EQ(ev.state, states[ev.index]);
        EXPECT_GE(ev.time_ps, last_ps);
        last_ps = ev.time_ps;
    }

    size_t count = 0;
    leds->on_led(nullptr);
    mod("system")->on_led([&](const led_event& ev) {
        EXPECT_EQ(&ev.leds, leds);
        count++;
    });

    size_t before = events.size();
    sess.step(100000, TIMEOUT_MS);
    EXPECT_GT(count, 0);
    EXPECT_EQ(events.size(), before);
    EXPECT_EQ(sess.events_dropped(), 0);
}

TEST_F(generator_test, on_uart) {
    vsp::module* term = mod("system.term0");
    string output;
    term->on_uart([&](const uart_event& ev) {
        EXPECT_EQ(&ev.terminal, term);
        output += ev.data;
    });

    sess.step(100000, TIMEOUT_MS);
    EXPECT_THAT(output, StartsWith("tick 0\ntick 1\n"));
    EXPECT_THAT(output, HasSubstr("tick 7\n"));
    EXPECT_EQ(sess.events_dropped(), 0);

    term->on_uart(nullptr);
    size_t len = output.size();
    sess.step(100000, TIMEOUT_MS);
    EXPECT_EQ(output.size(), len);
}

class fake_server
{
private:
    mwr::server_socket m_server;
    std::atomic<bool> m_done;
    std::thread m_thread;
    function<string(const string&)> m_respond;

    void run() {
        string packet;
        bool inside = false;
        while (!m_done) {
            int client = m_server.poll(10);
            if (client < 0)
                continue;

            try {
                char c = 0;
                m_server.recv(client, &c, 1);
                if (c == '$') {
                    packet.clear();
                    inside = true;
                } else if (c == '#' && inside) {
                    char checksum[2];
                    m_server.recv(client, checksum, sizeof(checksum));
                    string resp = m_respond(packet);
                    u8 sum = 0;
                    for (char ch : resp)
                        sum += (u8)ch;
                    m_server.send(client,
                                  mkstr("+$%s#%02x", resp.c_str(), sum));
                    inside = false;
                } else if (inside) {
                    packet += c;
                }
            } catch (std::exception&) {
                return; // client disconnected
            }
        }
    }

public:
    fake_server(function<string(const string&)> respond):
        m_server(1, 0, "localhost"),
        m_done(false),
        m_thread(),
        m_respond(std::move(respond)) {
        m_thread = std::thread(&fake_server::run, this);
    }

    ~fake_server() {
        m_done = true;
        m_thread.join();
    }

    u16 port() const { return m_server.port(); }
};

class dispatcher_test : public Test
{
protected:
    vector<string> commands;
    fake_server server;
    connection conn;
    dispatcher disp;
    vsp::module root;
    vsp::module* top;
    vsp::module* leds;
    vsp::module* out;
    vsp::module* term;
    unordered_map<string, vsp::module*> map;

    dispatcher_test():
        commands(),
        server([this](const string& cmd) {
            commands.push_back(cmd);
            return string("OK");
        }),
        conn("localhost", server.port()),
        disp(conn),
        root("", conn, disp, nullptr, "", ""),
        top(),
        leds(),
        out(),
        term(),
        map() {
        top = new vsp::module("top", conn, disp, &root, "", "");
        leds = new vsp::module("leds", conn, disp, top, "", "", { "led" });
        out = new vsp::module("out", conn, disp, top, "", "", { "trace" });
        term = new vsp::module("term", conn, disp, top, "", "", { "uart" });
        root.add_module(top);
        top->add_module(leds);
        top->add_module(out);
        top->add_module(term);
        map = { { "top", top },
                { "top.leds", leds },
                { "top.out", out },
                { "top.term", term } };
        disp.attach(&root, &map);
    }

    ~dispatcher_test() {
        disp.detach();
        conn.disconnect();
    }

    static string event(const string& name, const string& sender,
                        const string& payload) {
        return mkstr(
            "{\"event\":\"%s\",\"sender\":\"%s\",\"time\":10000,"
            "\"delta\":4,\"payload\":%s}",
            name.c_str(), sender.c_str(), payload.c_str());
    }
};

TEST_F(dispatcher_test, subscribe) {
    top->on_led([](const led_event& ev) {});
    top->on_led([](const led_event& ev) {}); // replacing sends nothing
    top->on_led(nullptr);
    root.on_trace([](const trace_info& info, const trace_payload& tx) {});
    root.on_trace<trace_tlm>([](const trace_info& info, const trace_tlm& tx) {
    });                     // already subscribed, sends nothing
    root.on_trace(nullptr); // tlm handler is still installed
    EXPECT_TRUE(root.is_traced());
    EXPECT_TRUE(top->has_leds());
    EXPECT_TRUE(top->has_uart());
    EXPECT_FALSE(leds->is_traceable());
    EXPECT_NO_THROW(leds->on_trace(nullptr)); // nothing installed, no command
    EXPECT_THAT(commands, ElementsAre("sub,led,top.leds", "unsub,led,top.leds",
                                      "sub,trace,top.out"));
}

TEST_F(dispatcher_test, shared_publishers) {
    auto fn = [](const trace_info& info, const trace_payload& tx) {};

    // handlers on a module and on its child need the same publisher, it is
    // only subscribed once and only unsubscribed when nobody needs it
    top->on_trace(fn);
    out->on_trace(fn);
    top->on_trace(nullptr);
    EXPECT_THAT(commands, ElementsAre("sub,trace,top.out"));

    out->on_trace(nullptr);
    EXPECT_THAT(commands,
                ElementsAre("sub,trace,top.out", "unsub,trace,top.out"));

    // objects without publishers below them cannot be subscribed
    EXPECT_THROW(leds->on_trace(fn), mwr::report);
    EXPECT_FALSE(leds->is_traced());
}

TEST_F(dispatcher_test, leds) {
    vector<string> seen;
    top->on_led([&](const led_event& ev) {
        EXPECT_EQ(&ev.leds, leds);
        EXPECT_EQ(ev.time_ps, 10000);
        EXPECT_EQ(ev.delta, 4);
        seen.push_back(mkstr("%zu=%d", ev.index, ev.state));
    });

    disp.dispatch(
        "{\"events\":[" +
        event("led", "top.leds", "{\"led\":2,\"state\":true}") + "," +
        event("led", "top.leds", "{\"led\":0,\"state\":false}") + "]}");
    EXPECT_THAT(seen, ElementsAre("2=1", "0=0"));
}

TEST_F(dispatcher_test, traces) {
    vector<u64> addresses;
    vector<string> protocols;
    size_t gpio = 0;

    top->on_trace<trace_tlm>([&](const trace_info& info, const trace_tlm& tx) {
        EXPECT_EQ(&info.port, out);
        EXPECT_TRUE(info.is_response());
        EXPECT_TRUE(info.error);
        addresses.push_back(tx.address);
    });
    top->on_trace<trace_gpio>(
        [&](const trace_info& info, const trace_gpio& tx) { gpio++; });
    root.on_trace([&](const trace_info& info, const trace_payload& tx) {
        std::visit(
            [&](const auto& p) {
                protocols.push_back(trace_protocol_str(p.PROTOCOL));
            },
            tx);
    });

    string tlm =
        "{\"dir\":\"bw\",\"protocol\":\"TLM\",\"error\":true,"
        "\"tx\":{\"address\":18446744073709551615,\"data\":[1],"
        "\"command\":\"READ\",\"byte_enable\":[],"
        "\"streaming_width\":1,\"dmi_allowed\":false,"
        "\"response_status\":\"TLM_OK_RESPONSE\"}}";
    string spi =
        "{\"dir\":\"fw\",\"protocol\":\"SPI\",\"error\":false,"
        "\"tx\":{\"mosi\":1,\"miso\":2}}";
    string bad =
        "{\"dir\":\"fw\",\"protocol\":\"TLM\",\"error\":false,"
        "\"tx\":{\"address\":16}}";
    string newp =
        "{\"dir\":\"fw\",\"protocol\":\"NEWPROTO\","
        "\"error\":false,\"tx\":{}}";
    disp.dispatch("{\"events\":[" + event("trace", "top.out", tlm) + "," +
                  event("trace", "top.out", spi) + "," +
                  event("trace", "top.out", bad) + "," +
                  event("trace", "top.out", newp) + "]}");

    EXPECT_THAT(addresses, ElementsAre(~0ull));
    EXPECT_EQ(gpio, 0); // spi goes to no typed handler, bad tlm is dropped
    EXPECT_THAT(protocols, ElementsAre("TLM", "SPI", "UNKNOWN"));
}

TEST_F(dispatcher_test, uart) {
    vector<string> data;
    top->on_uart([&](const uart_event& ev) {
        EXPECT_EQ(&ev.terminal, term);
        EXPECT_EQ(ev.time_ps, 10000);
        data.push_back(ev.data);
    });

    // consecutive characters are delivered together, other events split
    // them; bytes >= 0x80 and control characters arrive as \u00XX
    disp.dispatch("{\"events\":[" + event("uart", "top.term", "\"h\"") + "," +
                  event("uart", "top.term", "\"i\"") + "," +
                  event("uart", "top.term", "\"\\n\"") + "," +
                  event("led", "top.leds", "{\"led\":0,\"state\":true}") +
                  "," + event("uart", "top.term", "\"\\u00ff\"") + "," +
                  event("uart", "top.term", "\"\\u0000\"") + "]}");

    ASSERT_EQ(data.size(), 2);
    EXPECT_EQ(data[0], "hi\n");
    EXPECT_EQ(data[1], string("\xff\0", 2));
}

TEST_F(dispatcher_test, dropped_and_unknown) {
    u64 dropped = 0;
    size_t count = 0;
    disp.on_dropped([&](u64 n) { dropped += n; });
    top->on_led([&](const led_event& ev) { count++; });

    disp.dispatch("{\"events\":[" + event("led", "top.nothing", "{}") +
                  "],\"dropped\":3}");
    disp.dispatch("{\"events\":[]}");
    disp.dispatch("garbage");

    EXPECT_EQ(dropped, 3);
    EXPECT_EQ(disp.dropped(), 3);
    EXPECT_EQ(count, 0);
}

TEST_F(dispatcher_test, reconnect) {
    size_t count = 0;
    top->on_led([&](const led_event& ev) { count++; });

    disp.detach();
    commands.clear();
    disp.attach(&root, &map);
    EXPECT_THAT(commands, ElementsAre("sub,led,top.leds"));

    disp.dispatch("{\"events\":[" +
                  event("led", "top.leds", "{\"led\":0,\"state\":true}") +
                  "]}");
    EXPECT_EQ(count, 1);
}

TEST(events, unsupported) {
    // older simulators answer unknown commands with an empty response
    fake_server server([](const string& cmd) { return string(); });
    connection conn("localhost", server.port());
    dispatcher disp(conn);

    vsp::module root("", conn, disp, nullptr, "", "");
    vsp::module* top = new vsp::module("top", conn, disp, &root, "", "",
                                       { "trace" });
    root.add_module(top);

    try {
        top->on_trace([](const trace_info& info, const trace_payload& tx) {});
        ADD_FAILURE() << "subscribing did not throw";
    } catch (std::exception& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("not supported"));
    }

    EXPECT_FALSE(top->is_traced());
    conn.disconnect();
}

TEST(events, protocol_names) {
    EXPECT_STREQ(trace_protocol_str(VSP_TRACE_PROTOCOL_TLM), "TLM");
    EXPECT_STREQ(trace_protocol_str(VSP_TRACE_PROTOCOL_COUNT), "UNKNOWN");
    EXPECT_EQ(trace_protocol_from_str("ETHERNET"),
              VSP_TRACE_PROTOCOL_ETHERNET);
    EXPECT_EQ(trace_protocol_from_str("foo"), VSP_TRACE_PROTOCOL_UNKNOWN);
    EXPECT_EQ(trace_protocol_from_str(nullptr), VSP_TRACE_PROTOCOL_UNKNOWN);
    EXPECT_STREQ(trace_dir_str(VSP_TRACE_FW), "fw");
    EXPECT_STREQ(trace_dir_str(VSP_TRACE_BW), "bw");
}

template <typename T>
static bool parse(const string& text, T& tx) {
    json obj = json::parse(text, nullptr, false);
    return !obj.is_discarded() && parse_payload(obj, tx);
}

TEST(events, print) {
    EXPECT_EQ(format_time(0), "0.000000000s");
    EXPECT_EQ(format_time(1234567890123), "1.234567890s");

    stringstream ss;
    ss << trace_payload(trace_spi{ 1, 2 });
    EXPECT_EQ(ss.str(), "SPI mosi=0x01 miso=0x02");

    ss.str("");
    trace_tlm tlm{ "WRITE", 16,    { 0x11, 0, 0, 0 }, {},
                   4,       false, "TLM_OK_RESPONSE", std::nullopt };
    ss << trace_payload(tlm);
    EXPECT_EQ(ss.str(), "TLM WRITE @0x10 [11 00 00 00] TLM_OK_RESPONSE");

    ss.str("");
    ss << trace_payload(trace_unknown{ "NEWPROTO" });
    EXPECT_EQ(ss.str(), "NEWPROTO");
}

TEST(events, parse_tlm) {
    trace_tlm tx;
    ASSERT_TRUE(parse(
        R"({"address":16,"data":[17,0,0,0],"command":"WRITE",)"
        R"("byte_enable":[255],"streaming_width":4,"dmi_allowed":true,)"
        R"("response_status":"TLM_OK_RESPONSE","sbi":{"is_debug":true,)"
        R"("is_nodmi":false,"is_sync":false,"is_insn":true,"is_excl":false,)"
        R"("is_lock":false,"is_secure":true,"atype":"untranslated",)"
        R"("cpuid":1,"privilege":3,"asid":18446744073709551615}})",
        tx));
    EXPECT_TRUE(tx.is_write());
    EXPECT_EQ(tx.address, 16);
    EXPECT_THAT(tx.data, ElementsAre(17, 0, 0, 0));
    EXPECT_THAT(tx.byte_enable, ElementsAre(255));
    EXPECT_EQ(tx.streaming_width, 4);
    EXPECT_TRUE(tx.dmi_allowed);
    ASSERT_TRUE(tx.sbi);
    EXPECT_TRUE(tx.sbi->is_debug);
    EXPECT_TRUE(tx.sbi->is_insn);
    EXPECT_EQ(tx.sbi->cpuid, 1);
    EXPECT_EQ(tx.sbi->asid, ~0ull);

    EXPECT_FALSE(parse(R"({"address":16})", tx));
    EXPECT_FALSE(parse("garbage", tx));
}

TEST(events, parse_others) {
    trace_gpio gpio;
    ASSERT_TRUE(parse(R"({"state":true,"vector":3})", gpio));
    EXPECT_TRUE(gpio.state);
    EXPECT_EQ(gpio.gpio_vector, 3u);
    ASSERT_TRUE(parse(R"({"state":false})", gpio));
    EXPECT_FALSE(gpio.gpio_vector);

    trace_clk clk;
    ASSERT_TRUE(
        parse(R"({"period":10,"polarity":"posedge","duty_cycle":0.5})", clk));
    EXPECT_EQ(clk.period_ns, 10);
    EXPECT_TRUE(clk.posedge);
    EXPECT_DOUBLE_EQ(clk.duty_cycle, 0.5);

    trace_pci pci;
    ASSERT_TRUE(
        parse(R"({"command":"PCI_READ","response":"PCI_RESP_SUCCESS",)"
              R"("address_space":"PCI_AS_CFG","address":4,"data":[1,2],)"
              R"("debug":false})",
              pci));
    EXPECT_EQ(pci.address_space, "PCI_AS_CFG");
    EXPECT_THAT(pci.data, ElementsAre(1, 2));

    trace_i2c i2c;
    ASSERT_TRUE(parse(
        R"({"command":"I2C_START","response":"I2C_ACK","data":80})", i2c));
    EXPECT_EQ(i2c.data, 80);

    trace_lin lin;
    ASSERT_TRUE(
        parse(R"({"linid":5,"data":[1,2,3],"status":"LIN_SUCCESS"})", lin));
    EXPECT_EQ(lin.linid, 5);

    trace_spi spi;
    ASSERT_TRUE(parse(R"({"miso":1,"mosi":2})", spi));
    EXPECT_EQ(spi.miso, 1);
    EXPECT_EQ(spi.mosi, 2);

    trace_sd sd;
    ASSERT_TRUE(
        parse(R"({"command":"SD_CMD","opcode":"CMD17","argument":512,"crc":3,)"
              R"("spi":false,"response":[0,1],"status":"SD_OK"})",
              sd));
    EXPECT_EQ(sd.opcode, "CMD17");
    EXPECT_EQ(sd.argument, 512);
    EXPECT_THAT(sd.data, ElementsAre(0, 1));
    ASSERT_TRUE(parse(
        R"({"command":"SD_DATA_READ","data":66,"status":"SDTX_OK"})", sd));
    EXPECT_TRUE(sd.opcode.empty());
    EXPECT_THAT(sd.data, ElementsAre(66));

    trace_virtio virtio;
    ASSERT_TRUE(
        parse(R"({"index":1,"input_buffers":[{"addr":18446744073709551615,)"
              R"("size":8}],"output_buffers":[],"status":"VIRTIO_OK"})",
              virtio));
    ASSERT_EQ(virtio.input_buffers.size(), 1);
    EXPECT_EQ(virtio.input_buffers[0].addr, ~0ull);
    EXPECT_TRUE(virtio.output_buffers.empty());

    trace_serial serial;
    ASSERT_TRUE(parse(
        R"({"data":65,"bits":8,"baud":115200,"parity":"none","stop":"1"})",
        serial));
    EXPECT_EQ(serial.baud, 115200);

    trace_signal sig;
    ASSERT_TRUE(parse(R"({"data":5})", sig));
    EXPECT_EQ(std::get<u64>(sig.data), 5);
    ASSERT_TRUE(parse(R"({"data":-5})", sig));
    EXPECT_EQ(std::get<i64>(sig.data), -5);
    ASSERT_TRUE(parse(R"({"data":1.5})", sig));
    EXPECT_DOUBLE_EQ(std::get<double>(sig.data), 1.5);
    ASSERT_TRUE(parse(R"({"data":true})", sig));
    EXPECT_TRUE(std::get<bool>(sig.data));
    ASSERT_TRUE(parse(R"({"data":"123456789012345678901234"})", sig));
    EXPECT_EQ(std::get<string>(sig.data), "123456789012345678901234");
    ASSERT_TRUE(parse(R"({"data":null})", sig));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(sig.data));

    trace_ethernet eth;
    ASSERT_TRUE(parse(R"({"type":"IPv4","sourceaddr":"11:22:33:44:55:66",)"
                      R"("destaddr":"ff:ff:ff:ff:ff:ff","data":[1,2]})",
                      eth));
    EXPECT_EQ(eth.source, "11:22:33:44:55:66");
    ASSERT_TRUE(parse(R"({"type":"invalid"})", eth));
    EXPECT_TRUE(eth.source.empty());

    trace_can can;
    ASSERT_TRUE(parse(
        R"({"type":"CAN_FD","id":291,"xlf":false,"fdf":true,"eff":false,)"
        R"("brs":true,"esi":false,"data":[1,2,3]})",
        can));
    EXPECT_EQ(can.id, 291);
    EXPECT_TRUE(can.brs);
    EXPECT_FALSE(can.rtr);

    trace_usb usb;
    ASSERT_TRUE(
        parse(R"({"token":"USB_TOKEN_IN","addr":1,"endpoint":2,"data":[9],)"
              R"("status":"USB_RESULT_SUCCESS"})",
              usb));
    EXPECT_EQ(usb.endpoint, 2);
}
