/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

#include <future>

using namespace testing;
using namespace vsp;

class session_test : public Test
{
protected:
    static constexpr const char* HOST = "localhost";

    session_test(): sess(), subp(), port(connect_simple_vp(sess, subp)) {}

    virtual ~session_test() {
        sess.quit();
        subp.terminate();
    };

    vsp::session sess;
    mwr::subprocess subp;
    u16 port;
};

TEST_F(session_test, auto_discovery) {
    auto sessions = session::local_sessions();
    EXPECT_FALSE(sessions.empty());

    auto it = find_if(sessions.begin(), sessions.end(),
                      [pid = (u32)subp.pid()](const session_info& info) {
                          return info.pid == pid;
                      });
    ASSERT_NE(it, sessions.end());
    EXPECT_EQ(it->port, port);
    EXPECT_EQ(it->user, mwr::username());
    EXPECT_THAT(it->program, HasSubstr("simplevp"));
}

TEST_F(session_test, host_and_peer_data) {
    EXPECT_TRUE(sess.is_connected());
    EXPECT_STREQ(sess.host(), HOST);
    EXPECT_EQ(sess.port(), port);
    ASSERT_NE(sess.peer(), nullptr);
    EXPECT_GT(strlen(sess.peer()), 0);
}

TEST_F(session_test, connect_and_quit) {
    // double connect
    sess.connect(HOST, port);
    EXPECT_TRUE(sess.is_connected());

    sess.quit();
    EXPECT_FALSE(sess.is_connected());

    // double quit
    sess.quit();
    EXPECT_FALSE(sess.is_connected());
}

TEST_F(session_test, reconnect) {
    sess.disconnect();
    EXPECT_FALSE(sess.is_connected());

    sess.connect(HOST, port);
    EXPECT_TRUE(sess.is_connected());

    sess.quit();
    EXPECT_FALSE(sess.is_connected());

    EXPECT_THROW(sess.connect(HOST, port), mwr::report);
    EXPECT_FALSE(sess.is_connected());
}

TEST_F(session_test, dump) {
    internal::CaptureStdout();
    sess.dump();
    EXPECT_GT(internal::GetCapturedStdout().size(), 0);
}

TEST_F(session_test, versions) {
#ifdef GTEST_USES_SIMPLE_RE // windows simple regex: supports \d, not [] or {m}
    constexpr auto sc_regex = R"(\d+\.\d+\.\d+)";
    constexpr auto vcml_regex = R"(vcml-\d\d\d\d\.\d\d\.\d\d)";
#else // POSIX ERE on linux: use classes and {m}
    constexpr auto sc_regex = R"([0-9]+\.[0-9]+\.[0-9]+)";
    constexpr auto vcml_regex = R"(vcml-[0-9]{4}\.[0-9]{2}\.[0-9]{2})";
#endif

    EXPECT_THAT(sess.sysc_version(), ContainsRegex(sc_regex));
    EXPECT_THAT(sess.vcml_version(), ContainsRegex(vcml_regex));
}

TEST_F(session_test, time_and_cycles) {
    EXPECT_EQ(sess.get_time_ns(), 0);
    EXPECT_EQ(sess.get_cycle_count(), 1);
}

TEST_F(session_test, sessions) {
    EXPECT_FALSE(session::local_sessions().empty());
}

TEST_F(session_test, modules) {
    vsp::module* mod;

    mod = sess.find_module("");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->name(), "");
    EXPECT_NE(mod->children().size(), 0);

    mod = sess.find_module("system");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->name(), "system");
    EXPECT_STREQ(mod->version(), "v1.0");

    mod = sess.find_module("system.cpu0");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->name(), "cpu0");
    EXPECT_STREQ(mod->parent()->name(), "system");

    mod = sess.find_module("undefined-module");
    ASSERT_EQ(mod, nullptr);

    mod = sess.find_module("system.undefined-module");
    ASSERT_EQ(mod, nullptr);

    sess.quit();
    mod = sess.find_module("");
    ASSERT_EQ(mod, nullptr);
}

TEST_F(session_test, attributes) {
    attribute* attr;
    attr = sess.find_attribute("system.cpu0.arch");
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->get_str(), "riscv");

    vsp::module* cpu = sess.find_module("system.cpu0");
    ASSERT_NE(cpu, nullptr);
    EXPECT_NE(cpu->attributes().size(), 0);
    attr = cpu->find_attribute("arch");
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->get_str(), "riscv");
    EXPECT_EQ(attr->count(), 1);
    EXPECT_EQ(attr->type(), "string");

    attr->set(string("riscvi"));
    EXPECT_EQ(attr->get_str(), "riscvi");

    attr = cpu->find_attribute("undefined-attribute");
    EXPECT_EQ(attr, nullptr);

    attr = cpu->find_attribute("undefined-module.undefined-attribute");
    EXPECT_EQ(attr, nullptr);

    sess.quit();
    attr = sess.find_attribute("system.cpu0.arch");
    EXPECT_EQ(attr, nullptr);
}

TEST_F(session_test, attribute_types) {
    attribute* attr;

    attr = sess.find_attribute("system.cpu0.bool_property");
    ASSERT_NE(attr, nullptr);
    attr->set(true);
    EXPECT_EQ(attr->type(), "bool");
    EXPECT_EQ(attr->get_str(), "true");

    attr = sess.find_attribute("system.cpu0.i32_property");
    ASSERT_NE(attr, nullptr);
    attr->set((i32)1);
    EXPECT_EQ(attr->type(), "i32");
    EXPECT_EQ(attr->get_str(), "1");

    attr = sess.find_attribute("system.cpu0.i64_property");
    ASSERT_NE(attr, nullptr);
    attr->set((i64)2);
    EXPECT_EQ(attr->type(), "i64");
    EXPECT_EQ(attr->get_str(), "2");

    attr = sess.find_attribute("system.cpu0.u32_property");
    ASSERT_NE(attr, nullptr);
    attr->set((u32)3);
    EXPECT_EQ(attr->type(), "u32");
    EXPECT_EQ(attr->get_str(), "3");

    attr = sess.find_attribute("system.cpu0.u64_property");
    ASSERT_NE(attr, nullptr);
    attr->set((u64)4);
    EXPECT_EQ(attr->type(), "u64");
    EXPECT_EQ(attr->get_str(), "4");

    attr = sess.find_attribute("system.cpu0.float_property");
    ASSERT_NE(attr, nullptr);
    attr->set(5.5f);
    EXPECT_EQ(attr->type(), "float");
    EXPECT_EQ(attr->get_str(), "5.5");

    attr = sess.find_attribute("system.cpu0.double_property");
    ASSERT_NE(attr, nullptr);
    attr->set(6.25);
    EXPECT_EQ(attr->type(), "double");
    EXPECT_EQ(attr->get_str(), "6.25");

    attr = sess.find_attribute("system.cpu0.long_double_property");
    ASSERT_NE(attr, nullptr);
    attr->set(7.125);
    EXPECT_EQ(attr->type(), "unknown"); // currently not supported by VCML
    EXPECT_EQ(attr->get_str(), "7.125");

    attr = sess.find_attribute("system.cpu0.i32_vector_property");
    ASSERT_NE(attr, nullptr);
    vector<i32> i32_data(attr->count());
    for (size_t i = 0; i < i32_data.size(); ++i)
        i32_data[i] = static_cast<i32>(i);
    attr->set(i32_data);
    EXPECT_EQ(attr->get_str(), mwr::join(i32_data, ' '));

    attr = sess.find_attribute("system.cpu0.string_vector_property");
    ASSERT_NE(attr, nullptr);
    vector<string> string_data(attr->count());
    for (size_t i = 0; i < string_data.size(); ++i)
        string_data[i] = mwr::mkstr("val1: %zu", i);
    attr->set(string_data);
    for (auto& s : string_data)
        s = mwr::escape(s, " ");
    EXPECT_EQ(attr->get_str(), mwr::join(string_data, ' '));

    attr = sess.find_attribute("system.cpu0.string_property");
    ASSERT_NE(attr, nullptr);
    attr->set("test");
    EXPECT_EQ(attr->type(), "string");
    EXPECT_EQ(attr->get_str(), "test");

    // test escaping
    attr->set("$");
    EXPECT_EQ(attr->get_str(), "$");

    attr->set("#");
    EXPECT_EQ(attr->get_str(), "#");

    attr->set("*");
    EXPECT_EQ(attr->get_str(), "*");

    attr->set("}");
    EXPECT_EQ(attr->get_str(), "}");

    attr->set("$#*}");
    EXPECT_EQ(attr->get_str(), "$#*}");

    attr->set("a,b");
    EXPECT_EQ(attr->get_str(), "a,b");

    attr->set("\\");
    EXPECT_EQ(attr->get_str(), "\\");

    attr->set("\\n");
    EXPECT_EQ(attr->get_str(), "\\n");
}

TEST_F(session_test, attributes_while_running) {
    vsp::module* cpu = sess.find_module("system.cpu0");
    EXPECT_NE(cpu, nullptr);

    target* targ = sess.find_target("system.cpu0");
    ASSERT_NE(targ, nullptr);

    const vector<u8> inf_loop_inst{ 0x00, 0x00, 0x00, 0x20 };
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    attribute* attr = cpu->find_attribute("arch");

    sess.run();
    mwr::usleep(1000);
    EXPECT_EQ(attr->get_str(), "<error>");
    EXPECT_THROW(attr->set(string("riscvi")), mwr::report);
    sess.stop();
}

TEST_F(session_test, commands) {
    vsp::module* cpu = sess.find_module("system.cpu0");
    EXPECT_NE(cpu, nullptr);

    vsp::module* system = sess.find_module("system");
    EXPECT_NE(system, nullptr);

    auto& cmds = cpu->commands();
    EXPECT_NE(cmds.size(), 0);

    command* cmd;
    cmd = sess.find_command("system.cpu0.dump");
    EXPECT_NE(cmd, nullptr);
    cmd = system->find_command("cpu0.dump");
    EXPECT_NE(cmd, nullptr);

    cmd = cpu->find_command("undefined-command");
    EXPECT_EQ(cmd, nullptr);

    cmd = cpu->find_command("read");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->argc(), 3);
    EXPECT_NE(strlen(cmd->desc()), 0);
    EXPECT_THAT(cmd->execute({ "data", "0x0", "0x4" }),
                ContainsRegex("reading range"));
    EXPECT_THAT(cmd->execute(vector<string>{ "data", "0x0" }),
                ContainsRegex("need \\w+ arguments"));

    cmd = cpu->find_command("dump");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->argc(), 0);
    EXPECT_NE(strlen(cmd->desc()), 0);
    EXPECT_NE(cmd->execute().size(), 0);

    cmd = cpu->find_command("echo");
    ASSERT_NE(cmd, nullptr);
    EXPECT_STREQ(cmd->desc(), "echo command, used for testing");
    EXPECT_EQ(cmd->execute({ "a", "b", "c" }), "OK,a,b,c");

    sess.quit();

    cmd = sess.find_command("system.cpu0.dump");
    EXPECT_EQ(cmd, nullptr);
}

TEST(local_sessions, orphans) {
    // pid of a simulation that is gone, terminate waits for it to exit
    mwr::subprocess subp;
    u16 port = launch_simple_vp(subp);
    u32 pid = (u32)subp.pid();
    subp.terminate();

    string name = mkstr("vcml_session_%u", pid);
    string path = (fs::path(mwr::temp_dir()) / name).string();
    auto announce = [&](const string& user) {
        std::ofstream of(path);
        of << "localhost\n" << port << "\n" << user << "\nsimple_vp\n";
    };

    auto listed = [&]() {
        for (const auto& info : session::local_sessions()) {
            if (info.pid == pid)
                return true;
        }
        return false;
    };

    // files of other users are not listed, but also not removed
    announce("someone-else");
    EXPECT_FALSE(listed());
    EXPECT_TRUE(mwr::file_exists(path));

    // orphaned files of this user are removed
    announce(mwr::username());
    EXPECT_FALSE(listed());
    EXPECT_FALSE(mwr::file_exists(path));
}
