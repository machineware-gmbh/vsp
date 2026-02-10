/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

using namespace testing;
using namespace vsp;

class session_test : public Test
{
protected:
    session_test(): sess("localhost", 4444) {
        string path = mwr::dirname(mwr::progname());
        MWR_ERROR_ON(!subp.run(path + "/simple_vp"),
                     "failed to launch simple_vp");
        while (!sess.is_connected()) {
            mwr::usleep(5000);
            sess.connect();
        }
    }

    ~session_test() override {
        sess.quit();
        subp.terminate();
    };

    bool wait_for_target() {
        constexpr int ms_timeout = 10000;
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(ms_timeout);

        while (std::chrono::steady_clock::now() < deadline) {
            if (!sess.running())
                return true;
            mwr::usleep(1000);
        }
        return false;
    }

    const vector<u8> data1234{ 1, 2, 3, 4 };
    const vector<u8> inf_loop_inst{ 0x00, 0x00, 0x00, 0x20 };

    static constexpr u32 ADDR_IN_BOUND = 0x42;
    static constexpr u32 ADDR_OO_BOUND = 0x8000;

    session sess;

    mwr::subprocess subp;
};

TEST_F(session_test, host_and_peer_data) {
    EXPECT_TRUE(sess.is_connected());
    EXPECT_STREQ(sess.host(), "localhost");
    EXPECT_EQ(sess.port(), 4444);
    ASSERT_NE(sess.peer(), nullptr);
    EXPECT_GT(strlen(sess.peer()), 0);
}

TEST_F(session_test, connect_and_quit) {
    // double connect
    sess.connect();
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

    sess.connect();
    EXPECT_TRUE(sess.is_connected());

    sess.quit();
    EXPECT_FALSE(sess.is_connected());

    sess.connect();
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
    EXPECT_EQ(sess.time_ns(), 0);
    EXPECT_EQ(sess.cycle(), 1);
}

TEST_F(session_test, sessions) {
    EXPECT_NE(session::get_sessions().size(), 0);
}

TEST_F(session_test, targets) {
    auto& targets = sess.targets();
    EXPECT_EQ(targets.size(), 1);

    EXPECT_STREQ(targets.front().name(), "system.cpu");

    target* targ;
    targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    targ = sess.find_target("undefined-target");
    ASSERT_EQ(targ, nullptr);
}

TEST_F(session_test, modules) {
    vsp::module* mod;

    mod = sess.find_module("");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->name(), "");
    EXPECT_NE(mod->get_modules().size(), 0);

    mod = sess.find_module("system");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->name(), "system");
    EXPECT_STREQ(mod->version(), "v1.0");

    mod = sess.find_module("system.cpu");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->name(), "cpu");
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
    attr = sess.find_attribute("system.cpu.arch");
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->get_str(), "riscv");

    vsp::module* cpu = sess.find_module("system.cpu");
    ASSERT_NE(cpu, nullptr);
    EXPECT_NE(cpu->get_attributes().size(), 0);
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
    attr = sess.find_attribute("system.cpu.arch");
    EXPECT_EQ(attr, nullptr);
}

TEST_F(session_test, attribute_types) {
    attribute* attr;

    attr = sess.find_attribute("system.cpu.bool_property");
    ASSERT_NE(attr, nullptr);
    attr->set(true);
    EXPECT_EQ(attr->type(), "bool");
    EXPECT_EQ(attr->get_str(), "true");

    attr = sess.find_attribute("system.cpu.i32_property");
    ASSERT_NE(attr, nullptr);
    attr->set((i32)1);
    EXPECT_EQ(attr->type(), "i32");
    EXPECT_EQ(attr->get_str(), "1");

    attr = sess.find_attribute("system.cpu.i64_property");
    ASSERT_NE(attr, nullptr);
    attr->set((i64)2);
    EXPECT_EQ(attr->type(), "i64");
    EXPECT_EQ(attr->get_str(), "2");

    attr = sess.find_attribute("system.cpu.u32_property");
    ASSERT_NE(attr, nullptr);
    attr->set((u32)3);
    EXPECT_EQ(attr->type(), "u32");
    EXPECT_EQ(attr->get_str(), "3");

    attr = sess.find_attribute("system.cpu.u64_property");
    ASSERT_NE(attr, nullptr);
    attr->set((u64)4);
    EXPECT_EQ(attr->type(), "u64");
    EXPECT_EQ(attr->get_str(), "4");

    attr = sess.find_attribute("system.cpu.float_property");
    ASSERT_NE(attr, nullptr);
    attr->set(5.5f);
    EXPECT_EQ(attr->type(), "float");
    EXPECT_EQ(attr->get_str(), "5.5");

    attr = sess.find_attribute("system.cpu.double_property");
    ASSERT_NE(attr, nullptr);
    attr->set(6.25);
    EXPECT_EQ(attr->type(), "double");
    EXPECT_EQ(attr->get_str(), "6.25");

    attr = sess.find_attribute("system.cpu.long_double_property");
    ASSERT_NE(attr, nullptr);
    attr->set(7.125);
    EXPECT_EQ(attr->type(), "unknown"); // currently not supported by VCML
    EXPECT_EQ(attr->get_str(), "7.125");
}

TEST_F(session_test, attributes_while_running) {
    vsp::module* cpu = sess.find_module("system.cpu");
    EXPECT_NE(cpu, nullptr);

    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    attribute* attr = cpu->find_attribute("arch");

    sess.run();
    mwr::usleep(1000);
    EXPECT_EQ(attr->get_str(), "<error>");
    EXPECT_THROW(attr->set(string("riscvi")), mwr::report);
    sess.stop();
}

TEST_F(session_test, commands) {
    vsp::module* cpu = sess.find_module("system.cpu");
    EXPECT_NE(cpu, nullptr);

    vsp::module* system = sess.find_module("system");
    EXPECT_NE(system, nullptr);

    auto& cmds = cpu->get_commands();
    EXPECT_NE(cmds.size(), 0);

    command* cmd;
    cmd = sess.find_command("system.cpu.dump");
    EXPECT_NE(cmd, nullptr);
    cmd = system->find_command("cpu.dump");
    EXPECT_NE(cmd, nullptr);

    cmd = cpu->find_command("undefined-command");
    EXPECT_EQ(cmd, nullptr);

    cmd = cpu->find_command("read");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->argc(), 3);
    EXPECT_NE(strlen(cmd->desc()), 0);
    EXPECT_THAT(cmd->execute({ "data", "0x0", "0x4" }),
                ContainsRegex("reading range"));
    EXPECT_THAT(cmd->execute({ "data", "0", "0", "0" }),
                ContainsRegex("need \\w+ arguments"));

    cmd = cpu->find_command("dump");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->argc(), 0);
    EXPECT_NE(strlen(cmd->desc()), 0);
    EXPECT_NE(cmd->execute().size(), 0);

    sess.quit();

    cmd = sess.find_command("system.cpu.dump");
    EXPECT_EQ(cmd, nullptr);
}

TEST_F(session_test, commands_while_running) {
    vsp::module* cpu = sess.find_module("system.cpu");
    EXPECT_NE(cpu, nullptr);

    command* cmd = cpu->find_command("dump");
    EXPECT_NE(cmd, nullptr);

    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(cmd->execute(), std::runtime_error);
}

TEST_F(session_test, register_read) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    const char* reg_names[33] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc",
    };

    size_t i = 0;
    for (auto& reg : targ->regs()) {
        EXPECT_STREQ(reg.name(), reg_names[i]);

        vector<u8> ret;
        EXPECT_TRUE(reg.get_value(ret));
        EXPECT_THAT(ret, AllOf(SizeIs(4), Each(0)));

        i++;
    }

    u64 pc;
    targ->pc(pc);
    EXPECT_EQ(pc, 0);

    auto* reg = targ->find_reg("undefined-register");
    EXPECT_EQ(reg, nullptr);
}

TEST_F(session_test, register_write) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    cpureg* reg = targ->find_reg("a5");
    ASSERT_NE(reg, nullptr);
    EXPECT_TRUE(reg->set_value(data1234));

    // exceeding register width
    EXPECT_FALSE(reg->set_value({ 1, 2, 3, 4, 5 }));

    vector<u8> ret;
    EXPECT_TRUE(reg->get_value(ret));
    EXPECT_THAT(ret, ElementsAre(1, 2, 3, 4));

    // write into a non-writable registers
    reg = targ->find_reg("zero");
    ASSERT_NE(reg, nullptr);
    EXPECT_FALSE(reg->set_value(data1234));

    EXPECT_TRUE(reg->get_value(ret));
    EXPECT_THAT(ret, ElementsAre(0, 0, 0, 0));
}

TEST_F(session_test, register_size) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    cpureg* reg = targ->find_reg("a5");
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->size(), 4);

    reg = targ->find_reg("zero");
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->size(), 4);
}

TEST_F(session_test, memory_read) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(ADDR_IN_BOUND, data1234));

    // read in bounds
    EXPECT_THAT(targ->read_vmem(ADDR_IN_BOUND, 4), ElementsAre(1, 2, 3, 4));

    // read out of bounds
    EXPECT_THAT(targ->read_vmem(ADDR_OO_BOUND, 4), ElementsAre(0, 0, 0, 0));
}

TEST_F(session_test, memory_read_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    // read while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_EQ(targ->read_vmem(ADDR_IN_BOUND, 4).size(), 0);
    sess.stop();
}

TEST_F(session_test, memory_write) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    // write in bounds
    EXPECT_TRUE(targ->write_vmem(ADDR_IN_BOUND, data1234));

    // write out of bounds
    EXPECT_TRUE(targ->write_vmem(ADDR_OO_BOUND, data1234));
}

TEST_F(session_test, memory_write_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    // write while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_FALSE(targ->write_vmem(ADDR_IN_BOUND, data1234));
    sess.stop();
}

TEST_F(session_test, breakpoint) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    auto bp = targ->insert_breakpoint(0x04);
    EXPECT_TRUE(bp.has_value());
    EXPECT_EQ(bp.value().addr, 0x04);

    sess.run();
    ASSERT_TRUE(wait_for_target());

    u64 pc;
    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 4);
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_BREAKPOINT);

    EXPECT_TRUE(targ->remove_breakpoint(bp.value()));
}

TEST_F(session_test, breakpoint_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    sess.run();
    mwr::usleep(1000);
    auto bp = targ->insert_breakpoint(0x08);
    EXPECT_FALSE(bp.has_value());
    sess.stop();
}

TEST_F(session_test, watchpoint) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    auto wp_read = targ->insert_watchpoint(0x20, 0x04, WP_READ);
    EXPECT_TRUE(wp_read.has_value());
    EXPECT_EQ(wp_read.value().base, 0x20);
    EXPECT_EQ(wp_read.value().size, 0x04);

    auto wp_write = targ->insert_watchpoint(0x24, 0x04, WP_WRITE);
    EXPECT_TRUE(wp_write.has_value());
    EXPECT_EQ(wp_write.value().base, 0x24);
    EXPECT_EQ(wp_write.value().size, 0x04);

    vector<u8> inst_load{ 0x20, 0x01, 0x00, 0x11 };  // load from address 0x20
    vector<u8> inst_store{ 0x24, 0x01, 0x00, 0x10 }; // store to address 0x24
    EXPECT_TRUE(targ->write_vmem(0x0, inst_load));
    EXPECT_TRUE(targ->write_vmem(0x4, inst_store));

    sess.run();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);

    sess.run();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);

    EXPECT_TRUE(targ->remove_watchpoint(wp_read.value()));

    sess.run();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_WWATCHPOINT);

    EXPECT_TRUE(targ->remove_watchpoint(wp_write.value()));
}

TEST_F(session_test, watchpoint_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    sess.run();
    mwr::usleep(1000);
    auto wp = targ->insert_watchpoint(0x8, 0x04, WP_WRITE);
    EXPECT_FALSE(wp.has_value());
    sess.stop();
}

TEST_F(session_test, virt_to_phys) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_EQ(targ->virt_to_phys(0x04), 0x04);
}

TEST_F(session_test, virt_to_phys_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    sess.run();
    mwr::usleep(1000);
    EXPECT_EQ(targ->virt_to_phys(0x04), 0);
    sess.stop();
}

TEST_F(session_test, program_counter) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));
    u64 pc;

    // get pc while simulation is stopped
    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 0x0);

    // get pc while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_FALSE(targ->pc(pc));
    sess.stop();
}

TEST_F(session_test, stepping) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    u64 pc;

    EXPECT_TRUE(targ->write_vmem(0x0, { 0x0, 0x0, 0x0, 0x0 }));     // nop
    EXPECT_TRUE(targ->write_vmem(0x4, { 0x0, 0x0, 0x0, 0x0 }));     // nop
    EXPECT_TRUE(targ->write_vmem(0x8, { 0x0, 0x0, 0x0, 0x0 }));     // nop
    EXPECT_TRUE(targ->write_vmem(0xc, { 0xf4, 0xff, 0xff, 0x20 })); // back

    sess.stepi(*targ);
    ASSERT_TRUE(wait_for_target());

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_STEP_COMPLETE);
    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 0x4);

    targ->step(3);
    ASSERT_TRUE(wait_for_target());

    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 0x0);

    targ->step();
    ASSERT_TRUE(wait_for_target());
    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 0x4);

    // mocking a very slow simulation
    attribute* wait_per_inst = sess.find_attribute("system.cpu.wait_per_inst");
    ASSERT_NE(wait_per_inst, nullptr);
    wait_per_inst->set(500'000ull);
    targ->step(3);
    ASSERT_TRUE(wait_for_target());
    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 0x0);

    // only one step is performed because the simulation is very slow
    targ->step();
    targ->step();
    ASSERT_TRUE(wait_for_target());
    EXPECT_TRUE(targ->pc(pc));
    EXPECT_EQ(pc, 0x4);
}

TEST_F(session_test, stop_with_wait) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    sess.run();
    mwr::usleep(1000);
    sess.stop();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_USER);
}

TEST_F(session_test, stop_immediately) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_TRUE(targ->write_vmem(0x0, inf_loop_inst));

    sess.run();
    sess.stop();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_USER);
}

TEST_F(session_test, multi_session) {
    session sess2("localhost", 4444);
    while (!sess2.is_connected()) {
        mwr::usleep(5000);
        sess2.connect();
    }

    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    target* targ2 = sess2.find_target("system.cpu");
    ASSERT_NE(targ2, nullptr);

    sess2.quit();
}
