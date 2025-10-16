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

#include <future>

using namespace vsp;

using testing::AllOf;
using testing::Each;
using testing::ElementsAre;
using testing::SizeIs;

class session_test : public testing::Test
{
protected:
    session_test(): sess("localhost", 4444) {
        subp.run("./simple_system");
        while (!sess.is_connected()) {
            mwr::usleep(10000);
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

    mwr::subprocess subp;
    session sess;
};

TEST_F(session_test, connect) {
    EXPECT_STREQ(sess.host(), "localhost");
    EXPECT_EQ(sess.port(), 4444);
    EXPECT_GT(strlen(sess.peer()), 0);
    EXPECT_TRUE(sess.is_connected());
}

TEST_F(session_test, versions) {
    EXPECT_NE(sess.sysc_version().size(), 0);
    EXPECT_NE(sess.vcml_version().size(), 0);
    EXPECT_EQ(sess.time_ns(), 0);
    EXPECT_EQ(sess.cycle(), 1);
    sess.disconnect();
}

TEST_F(session_test, find_target) {
    auto* target = sess.find_target("system.cpu");
    EXPECT_NE(target, nullptr);
}

TEST_F(session_test, targets) {
    auto& targets = sess.targets();
    EXPECT_EQ(targets.size(), 1);
    target& t = targets.front();
    EXPECT_STREQ(t.name(), "system.cpu");
}

TEST_F(session_test, modules) {
    module* mod = sess.find_module("");
    EXPECT_STREQ(mod->name(), "");
    auto& mods = mod->get_modules();
    EXPECT_STREQ(mods.front()->name(), "system");
}

TEST_F(session_test, register_read) {
    auto& targets = sess.targets();
    target& t = targets.front();

    static std::string reg_names[33] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc"
    };

    size_t i = 0;
    for (auto& reg : t.regs()) {
        EXPECT_STREQ(reg.name(), reg_names[i].c_str());

        vector<u8> ret;
        EXPECT_TRUE(reg.get_value(ret));
        EXPECT_THAT(ret, AllOf(SizeIs(4), Each(0)));

        i++;
    }

    // get program counter
    u64 pc;
    t.pc(pc);
    EXPECT_EQ(pc, 0);

    // read non-existing register
    auto* reg = t.find_reg("non-existing");
    EXPECT_EQ(reg, nullptr);
}

TEST_F(session_test, register_write) {
    auto& targets = sess.targets();
    target& t = targets.front();

    // write into a writable register
    auto* reg = t.find_reg("a5");
    reg->set_value({ 1, 2, 3, 4 });

    vector<u8> ret;
    EXPECT_TRUE(reg->get_value(ret));
    EXPECT_THAT(ret, ElementsAre(1, 2, 3, 4));

    // write into a non-writable registers
    reg = t.find_reg("zero");
    reg->set_value({ 1, 2, 3, 4 });

    EXPECT_TRUE(reg->get_value(ret));
    EXPECT_THAT(ret, ElementsAre(0, 0, 0, 0));
}

TEST_F(session_test, memory_read_write) {
    auto& targets = sess.targets();
    target& t = targets.front();
    constexpr u32 addr_in_bound = 0x42;
    constexpr u32 addr_oo_bound = 0x8000;

    vector<u8> data{ 1, 2, 3, 4 };
    vector<u8> ret;

    // write and read in bounds
    EXPECT_TRUE(t.write_vmem(addr_in_bound, data));
    EXPECT_THAT(t.read_vmem(addr_in_bound, 4), ElementsAre(1, 2, 3, 4));

    // write and read out of bounds
    EXPECT_TRUE(t.write_vmem(addr_oo_bound, data));
    EXPECT_THAT(t.read_vmem(addr_oo_bound, 4), ElementsAre(0, 0, 0, 0));
}

TEST_F(session_test, breakpoint) {
    auto& targets = sess.targets();
    target& t = targets.front();

    auto bp = t.insert_breakpoint(0x04);
    EXPECT_TRUE(bp.has_value());
    EXPECT_EQ(bp.value().addr, 0x04);

    sess.run();
    wait_for_target();

    u64 pc;
    EXPECT_TRUE(t.pc(pc));
    EXPECT_EQ(pc, 4);
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_BREAKPOINT);

    EXPECT_TRUE(t.remove_breakpoint(bp.value()));
}

TEST_F(session_test, watchpoint) {
    auto& targets = sess.targets();
    target& t = targets.front();

    auto wp_read = t.insert_watchpoint(0x04, 0x04, WP_READ);
    EXPECT_TRUE(wp_read.has_value());
    EXPECT_EQ(wp_read.value().base, 0x04);
    EXPECT_EQ(wp_read.value().size, 0x04);

    auto wp_write = t.insert_watchpoint(0x08, 0x04, WP_WRITE);
    EXPECT_TRUE(wp_write.has_value());
    EXPECT_EQ(wp_write.value().base, 0x08);
    EXPECT_EQ(wp_write.value().size, 0x04);

    vector<u8> inst_load{ 0x4, 0x01, 0x00, 0x11 };  // load from address 0x4
    vector<u8> inst_store{ 0x8, 0x01, 0x00, 0x10 }; // store to address 0x8
    EXPECT_TRUE(t.write_vmem(0x0, inst_load));
    EXPECT_TRUE(t.write_vmem(0x4, inst_store));

    sess.run();
    wait_for_target();
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);

    sess.run();
    wait_for_target();
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_WWATCHPOINT);

    EXPECT_TRUE(t.remove_watchpoint(wp_read.value()));
    EXPECT_TRUE(t.remove_watchpoint(wp_write.value()));
}

TEST_F(session_test, virt_to_phys) {
    auto& targets = sess.targets();
    target& t = targets.front();

    EXPECT_EQ(t.virt_to_phys(0x04), 0x04);
}

TEST_F(session_test, stepping) {
    auto& targets = sess.targets();
    target& t = targets.front();
    u64 pc;

    EXPECT_TRUE(t.write_vmem(0x0, { 0x4 }));
    EXPECT_TRUE(t.write_vmem(0x4, { 0x8 }));
    EXPECT_TRUE(t.write_vmem(0x8, { 0xb }));
    EXPECT_TRUE(t.write_vmem(0xc, { 0xf4, 0xff, 0xff, 0x20 })); // back to 0

    sess.stepi(t);
    wait_for_target();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_STEP_COMPLETE);
    EXPECT_TRUE(t.pc(pc));
    EXPECT_EQ(pc, 0x4);

    t.step(3);
    wait_for_target();

    EXPECT_TRUE(t.pc(pc));
    EXPECT_EQ(pc, 0x0);

    sess.step(3000, true);
    EXPECT_EQ(sess.reason().reason,
              VSP_STOP_REASON_UNKNOWN); // TODO: Really like this?
}

TEST_F(session_test, stop) {
    auto& targets = sess.targets();
    target& t = targets.front();

    EXPECT_TRUE(t.write_vmem(0x0, { 0x4 }));
    EXPECT_TRUE(t.write_vmem(0x4, { 0x8 }));
    EXPECT_TRUE(t.write_vmem(0x8, { 0xb }));
    EXPECT_TRUE(t.write_vmem(0xc, { 0xf4, 0xff, 0xff, 0x20 })); // back to 0

    sess.run();
    sess.stop();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_USER);
}
