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
    session_test(): subp(), sess("localhost", 4444) {
        subp.run("./simple_vp");
        while (!sess.is_connected()) {
            mwr::usleep(10000);
            sess.connect();
        }
    }

    ~session_test() override {
        sess.disconnect();
        subp.terminate();
    };

    mwr::subprocess subp;
    session sess;
};

TEST_F(session_test, connect) {
    EXPECT_STREQ(sess.host(), "localhost");
    EXPECT_EQ(sess.port(), 4444);
    EXPECT_STREQ(sess.peer(), "127.0.0.1:4444");
    EXPECT_TRUE(sess.is_connected());
}

TEST_F(session_test, versions) {
    EXPECT_NE(sess.sysc_version().size(), 0);
    EXPECT_NE(sess.vcml_version().size(), 0);
    EXPECT_EQ(sess.time_ns(), 0);
    EXPECT_EQ(sess.cycle(), 1);
    sess.disconnect();
}

TEST_F(session_test, targets) {
    auto& targets = sess.targets();
    EXPECT_EQ(targets.size(), 1);
    target& t = targets.front();
    EXPECT_EQ(std::string(t.name()), std::string("system.cpu"));
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
        EXPECT_EQ(std::string(reg.name()), reg_names[i]);

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
