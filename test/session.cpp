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

using namespace vsp;

using testing::AllOf;
using testing::ContainsRegex;
using testing::Each;
using testing::ElementsAre;
using testing::SizeIs;

class session_test : public testing::Test
{
protected:
    session_test(): sess("localhost", 4444) {
        subp.run("simple_system");
        while (!sess.is_connected()) {
            mwr::usleep(5000);
            sess.connect();
        }
    }

    ~session_test() override {
        sess.disconnect();
        subp.terminate();
    };

    void wait_for_target() {
        while (sess.running()) {
            mwr::usleep(1000);
        }
    }

    const vector<u8> inf_loop_inst{ 0x00, 0xff, 0xff, 0x20 };
    session sess;
    mwr::subprocess subp;
};

TEST_F(session_test, connect_and_quit) {
    EXPECT_STREQ(sess.host(), "localhost");
    EXPECT_EQ(sess.port(), 4444);
    EXPECT_GT(strlen(sess.peer()), 0);
    EXPECT_TRUE(sess.is_connected());

    // double connect
    sess.connect();
    EXPECT_TRUE(sess.is_connected());

    sess.quit();
    EXPECT_FALSE(sess.is_connected());

    // double quit
    sess.quit();
    EXPECT_FALSE(sess.is_connected());
}

TEST_F(session_test, dump) {
    testing::internal::CaptureStdout();
    sess.dump();
    string out = testing::internal::GetCapturedStdout();
    EXPECT_GT(out.size(), 0);
}

TEST_F(session_test, versions) {
    const char* sc_regex = "[0-9]+.[0-9]+.[0-9]+";
    EXPECT_THAT(sess.sysc_version(), ContainsRegex(sc_regex));
    const char* vcml_regex = "vcml-[0-9]{4}.[0-9]{2}.[0-9]{2}";
    EXPECT_THAT(sess.vcml_version(), ContainsRegex(vcml_regex));
}

TEST_F(session_test, time_and_cycles) {
    EXPECT_EQ(sess.time_ns(), 0);
    EXPECT_EQ(sess.cycle(), 1);
}

TEST_F(session_test, targets) {
    auto& targets = sess.targets();
    EXPECT_EQ(targets.size(), 1);

    EXPECT_STREQ(targets.front().name(), "system.cpu");

    target* t;
    t = sess.find_target("system.cpu");
    EXPECT_NE(t, nullptr);

    t = sess.find_target("undefined-target");
    EXPECT_EQ(t, nullptr);
}

TEST_F(session_test, modules) {
    module* mod;

    mod = sess.find_module("");
    EXPECT_STREQ(mod->name(), "");

    auto* system = mod->get_modules().front();
    EXPECT_STREQ(system->name(), "system");
    EXPECT_STREQ(system->version(), "v1.0");

    auto* cpu = system->get_modules().front();
    EXPECT_STREQ(cpu->name(), "cpu");
    EXPECT_STREQ(cpu->parent()->name(), "system");

    auto& attrs = cpu->get_attritbutes();
    EXPECT_NE(attrs.size(), 0);

    mod = sess.find_module("undefined-module");
    EXPECT_EQ(mod, nullptr);
}

TEST_F(session_test, attributes) {
    module* mod = sess.find_module("");
    auto* cpu = mod->get_modules().front()->get_modules().front();
    EXPECT_STREQ(cpu->name(), "cpu");

    auto& attrs = cpu->get_attritbutes();
    EXPECT_NE(attrs.size(), 0);

    attribute* attr;
    attr = cpu->find_attribute("arch");
    EXPECT_NE(attr, nullptr);
    EXPECT_EQ(attr->get_str(), "riscv");
    EXPECT_EQ(attr->count(), 1);

    attr->set(string("riscvi"));
    EXPECT_EQ(attr->get_str(), "riscvi");

    attr = cpu->find_attribute("undefined-attribute");
    EXPECT_EQ(attr, nullptr);
}

TEST_F(session_test, attributes_while_running) {
    module* mod = sess.find_module("");
    auto* cpu = mod->get_modules().front()->get_modules().front();
    EXPECT_STREQ(cpu->name(), "cpu");

    auto& attrs = cpu->get_attritbutes();
    EXPECT_NE(attrs.size(), 0);

    attribute* attr = cpu->find_attribute("arch");

    target* t = sess.find_target("system.cpu");
    EXPECT_TRUE(t->write_vmem(0x0, inf_loop_inst));

    sess.run();
    EXPECT_EQ(attr->get_str(), "<error>");
    EXPECT_THROW(attr->set(string("riscvi")), mwr::report);
    sess.stop();
}

TEST_F(session_test, commands) {
    module* mod = sess.find_module("");
    auto* cpu = mod->get_modules().front()->get_modules().front();
    EXPECT_STREQ(cpu->name(), "cpu");

    command* cmd;

    auto& cmds = cpu->get_commands();
    EXPECT_NE(cmds.size(), 0);

    cmd = cpu->find_command("dump");
    EXPECT_NE(cmd, nullptr);

    auto dreg = "\\w+:\n  PC \\w+\n  LR \\w+\n  SP \\w+\n  ID \\w+\n\\w+:\n";
    EXPECT_THAT(cmd->execute(), ContainsRegex(dreg));

    cmd = cpu->find_command("undefined-command");
    EXPECT_EQ(cmd, nullptr);
}

TEST_F(session_test, commands_while_running) {
    module* mod = sess.find_module("");
    auto* cpu = mod->get_modules().front()->get_modules().front();
    EXPECT_STREQ(cpu->name(), "cpu");

    command* cmd = cpu->find_command("dump");
    EXPECT_NE(cmd, nullptr);

    target* t = sess.find_target("system.cpu");
    EXPECT_TRUE(t->write_vmem(0x0, inf_loop_inst));

    sess.run();
    EXPECT_THROW(cmd->execute(), std::runtime_error);
    sess.stop();
}

TEST_F(session_test, register_read) {
    target* t = sess.find_target("system.cpu");

    const char* reg_names[33] = { "zero", "ra", "sp", "gp", "tp", "t0",  "t1",
                                  "t2",   "s0", "s1", "a0", "a1", "a2",  "a3",
                                  "a4",   "a5", "a6", "a7", "s2", "s3",  "s4",
                                  "s5",   "s6", "s7", "s8", "s9", "s10", "s11",
                                  "t3",   "t4", "t5", "t6", "pc" };

    size_t i = 0;
    for (auto& reg : t->regs()) {
        EXPECT_STREQ(reg.name(), reg_names[i]);

        vector<u8> ret;
        EXPECT_TRUE(reg.get_value(ret));
        EXPECT_THAT(ret, AllOf(SizeIs(4), Each(0)));

        i++;
    }

    u64 pc;
    t->pc(pc);
    EXPECT_EQ(pc, 0);

    auto* reg = t->find_reg("undefined-registers");
    EXPECT_EQ(reg, nullptr);
}

TEST_F(session_test, register_write) {
    target* t = sess.find_target("system.cpu");

    auto* reg = t->find_reg("a5");
    EXPECT_TRUE(reg->set_value({ 1, 2, 3, 4 }));

    vector<u8> ret;
    EXPECT_TRUE(reg->get_value(ret));
    EXPECT_THAT(ret, ElementsAre(1, 2, 3, 4));

    // write into a non-writable registers
    reg = t->find_reg("zero");
    EXPECT_FALSE(reg->set_value({ 1, 2, 3, 4 }));

    EXPECT_TRUE(reg->get_value(ret));
    EXPECT_THAT(ret, ElementsAre(0, 0, 0, 0));
}

TEST_F(session_test, register_size) {
    target* t = sess.find_target("system.cpu");

    auto* reg = t->find_reg("a5");
    EXPECT_EQ(reg->size(), 4);

    reg = t->find_reg("zero");
    EXPECT_EQ(reg->size(), 4);
}

TEST_F(session_test, memory_read_write) {
    target* t = sess.find_target("system.cpu");
    vector<u8> data{ 1, 2, 3, 4 };
    vector<u8> ret;

    // write and read in bounds
    constexpr u32 addr_in_bound = 0x42;
    EXPECT_TRUE(t->write_vmem(addr_in_bound, data));
    EXPECT_THAT(t->read_vmem(addr_in_bound, 4), ElementsAre(1, 2, 3, 4));

    // write and read out of bounds
    constexpr u32 addr_oo_bound = 0x8000;
    EXPECT_TRUE(t->write_vmem(addr_oo_bound, data));
    EXPECT_THAT(t->read_vmem(addr_oo_bound, 4), ElementsAre(0, 0, 0, 0));
}

TEST_F(session_test, breakpoint) {
    target* t = sess.find_target("system.cpu");

    auto bp = t->insert_breakpoint(0x04);
    EXPECT_TRUE(bp.has_value());
    EXPECT_EQ(bp.value().addr, 0x04);

    sess.run();
    wait_for_target();

    u64 pc;
    EXPECT_TRUE(t->pc(pc));
    EXPECT_EQ(pc, 4);
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_BREAKPOINT);

    EXPECT_TRUE(t->remove_breakpoint(bp.value()));
}

TEST_F(session_test, breakpoint_while_running) {
    target* t = sess.find_target("system.cpu");
    EXPECT_TRUE(t->write_vmem(0x0, inf_loop_inst));

    sess.run();
    auto bp = t->insert_breakpoint(0x08);
    EXPECT_FALSE(bp.has_value());
    sess.stop();
}

TEST_F(session_test, watchpoint) {
    target* t = sess.find_target("system.cpu");

    auto wp_read = t->insert_watchpoint(0x20, 0x04, WP_READ);
    EXPECT_TRUE(wp_read.has_value());
    EXPECT_EQ(wp_read.value().base, 0x20);
    EXPECT_EQ(wp_read.value().size, 0x04);

    auto wp_write = t->insert_watchpoint(0x24, 0x04, WP_WRITE);
    EXPECT_TRUE(wp_write.has_value());
    EXPECT_EQ(wp_write.value().base, 0x24);
    EXPECT_EQ(wp_write.value().size, 0x04);

    vector<u8> inst_load{ 0x20, 0x01, 0x00, 0x11 };  // load from address 0x20
    vector<u8> inst_store{ 0x24, 0x01, 0x00, 0x10 }; // store to address 0x24
    EXPECT_TRUE(t->write_vmem(0x0, inst_load));
    EXPECT_TRUE(t->write_vmem(0x4, inst_store));

    sess.run();
    wait_for_target();
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);

    sess.run();
    wait_for_target();
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);

    EXPECT_TRUE(t->remove_watchpoint(wp_read.value()));

    sess.run();
    wait_for_target();
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_WWATCHPOINT);

    EXPECT_TRUE(t->remove_watchpoint(wp_write.value()));
}

TEST_F(session_test, watchpoint_while_running) {
    target* t = sess.find_target("system.cpu");
    EXPECT_TRUE(t->write_vmem(0x0, inf_loop_inst));

    sess.run();
    auto wp = t->insert_watchpoint(0x8, 0x04, WP_WRITE);
    EXPECT_FALSE(wp.has_value());
    sess.stop();
}

TEST_F(session_test, virt_to_phys) {
    target* t = sess.find_target("system.cpu");

    EXPECT_EQ(t->virt_to_phys(0x04), 0x04);
}

TEST_F(session_test, virt_to_phys_while_running) {
    target* t = sess.find_target("system.cpu");
    EXPECT_TRUE(t->write_vmem(0x0, inf_loop_inst));

    sess.run();
    EXPECT_EQ(t->virt_to_phys(0x04), 0);
    sess.stop();
}

TEST_F(session_test, stepping) {
    target* t = sess.find_target("system.cpu");;
    u64 pc;

    EXPECT_TRUE(t->write_vmem(0x0, { 0x0, 0x0, 0x0, 0x0 }));     // nop
    EXPECT_TRUE(t->write_vmem(0x4, { 0x0, 0x0, 0x0, 0x0 }));     // nop
    EXPECT_TRUE(t->write_vmem(0x8, { 0x0, 0x0, 0x0, 0x0 }));     // nop
    EXPECT_TRUE(t->write_vmem(0xc, { 0xf4, 0xff, 0xff, 0x20 })); // back to 0

    sess.stepi(*t);
    wait_for_target();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_STEP_COMPLETE);
    EXPECT_TRUE(t->pc(pc));
    EXPECT_EQ(pc, 0x4);

    t->step(3);
    wait_for_target();

    EXPECT_TRUE(t->pc(pc));
    EXPECT_EQ(pc, 0x0);

    sess.step(5000, true);
    EXPECT_EQ(sess.reason().reason,
              VSP_STOP_REASON_UNKNOWN); // TODO: really unknown?
}

TEST_F(session_test, stop) {
    target* t = sess.find_target("system.cpu");

    EXPECT_TRUE(t->write_vmem(0x0, inf_loop_inst));

    sess.run();
    mwr::usleep(1000);
    sess.stop();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_USER);
}

// TODOs: immediate stop: there seems to be some kind of race condition
//
//        read/write watchpoint:
//
//        system exponential backoff: maybe use some kind of command to make the CPU slower?
