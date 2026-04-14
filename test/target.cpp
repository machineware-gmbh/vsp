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

using namespace testing;
using namespace vsp;

class target_test : public Test
{
protected:
    static constexpr const char* HOST = "localhost";
    static constexpr u16 PORT = 54321;

    target_test(): sess(), subp() {
        string exec = SIMPLE_VP_PATH;
        vector<string> args{ "-c", mkstr("system.session=%hu", PORT) };
        MWR_ERROR_ON(!subp.run(exec, args), "failed to launch simple_vp");
        try_connect(sess, HOST, PORT, 100);
    }

    virtual ~target_test() {
        sess.quit();
        subp.terminate();
    };

    bool wait_for_target() { return wait_for_target(sess); }
    bool wait_for_target(session& s) {
        constexpr int ms_timeout = 10000;
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(ms_timeout);

        while (std::chrono::steady_clock::now() < deadline) {
            if (!s.check_running())
                return true;
            mwr::usleep(100);
        }
        return false;
    }

    const vector<u8> data1234{ 1, 2, 3, 4 };
    const vector<u8> inf_loop_inst{ 0x00, 0x00, 0x00, 0x20 };

    static constexpr u32 ADDR_IN_BOUND = 0x42;
    static constexpr u32 ADDR_OO_BOUND = 0x8000;

    vsp::session sess;
    mwr::subprocess subp;
};

TEST_F(target_test, targets) {
    auto targets = sess.targets();
    EXPECT_EQ(targets.size(), 1);

    EXPECT_STREQ(targets.front()->name(), "system.cpu");

    target* targ;
    targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    targ = sess.find_target("undefined-target");
    ASSERT_EQ(targ, nullptr);
}

TEST_F(target_test, modules) {
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

TEST_F(target_test, attributes) {
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

TEST_F(target_test, attribute_types) {
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

    attr = sess.find_attribute("system.cpu.i32_vector_property");
    ASSERT_NE(attr, nullptr);
    vector<i32> i32_data(attr->count());
    for (size_t i = 0; i < i32_data.size(); ++i)
        i32_data[i] = static_cast<i32>(i);
    attr->set(i32_data);
    EXPECT_EQ(attr->get_str(), mwr::join(i32_data, ' '));

    attr = sess.find_attribute("system.cpu.string_vector_property");
    ASSERT_NE(attr, nullptr);
    vector<string> string_data(attr->count());
    for (size_t i = 0; i < string_data.size(); ++i)
        string_data[i] = mwr::mkstr("val1: %zu", i);
    attr->set(string_data);
    EXPECT_EQ(attr->get_str(), mwr::join(string_data, ' '));

    attr = sess.find_attribute("system.cpu.string_property");
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

    attr->set("\\n");
    EXPECT_EQ(attr->get_str(), "\\n");
}

TEST_F(target_test, attributes_while_running) {
    vsp::module* cpu = sess.find_module("system.cpu");
    EXPECT_NE(cpu, nullptr);

    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    attribute* attr = cpu->find_attribute("arch");

    sess.run();
    mwr::usleep(1000);
    EXPECT_EQ(attr->get_str(), "<error>");
    EXPECT_THROW(attr->set(string("riscvi")), mwr::report);
    sess.stop();
}

TEST_F(target_test, commands) {
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

TEST_F(target_test, commands_while_running) {
    vsp::module* cpu = sess.find_module("system.cpu");
    EXPECT_NE(cpu, nullptr);

    command* cmd = cpu->find_command("dump");
    EXPECT_NE(cmd, nullptr);

    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(cmd->execute(), mwr::report);
}

TEST_F(target_test, register_read) {
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
        reg.get_value(ret);
        EXPECT_THAT(ret, AllOf(SizeIs(4), Each(0)));

        i++;
    }

    u64 pc = targ->get_pc();
    EXPECT_EQ(pc, 0);

    auto* reg = targ->find_reg("undefined-register");
    EXPECT_EQ(reg, nullptr);
}

TEST_F(target_test, register_write) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    cpureg* reg = targ->find_reg("a5");
    ASSERT_NE(reg, nullptr);
    reg->set_value(data1234);

    // exceeding register width
    EXPECT_THROW(reg->set_value({ 1, 2, 3, 4, 5 }), mwr::report);

    vector<u8> ret;
    reg->get_value(ret);
    EXPECT_THAT(ret, ElementsAre(1, 2, 3, 4));

    // write into a non-writable registers
    reg = targ->find_reg("zero");
    ASSERT_NE(reg, nullptr);
    EXPECT_THROW(reg->set_value(data1234), mwr::report);

    reg->get_value(ret);
    EXPECT_THAT(ret, ElementsAre(0, 0, 0, 0));
}

TEST_F(target_test, register_size) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    cpureg* reg = targ->find_reg("a5");
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->size(), 4);

    reg = targ->find_reg("zero");
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->size(), 4);
}

TEST_F(target_test, memory_read) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(ADDR_IN_BOUND, data1234), 0);

    // read in bounds
    EXPECT_THAT(targ->read_vmem(ADDR_IN_BOUND, 4), ElementsAre(1, 2, 3, 4));

    // read out of bounds
    EXPECT_THAT(targ->read_vmem(ADDR_OO_BOUND, 4), ElementsAre(0, 0, 0, 0));
}

TEST_F(target_test, memory_read_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    // read while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->read_vmem(ADDR_IN_BOUND, 4), mwr::report);
    sess.stop();
}

TEST_F(target_test, memory_write) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    // write in bounds
    EXPECT_NE(targ->write_vmem(ADDR_IN_BOUND, data1234), 0);

    // write out of bounds
    EXPECT_EQ(targ->write_vmem(ADDR_OO_BOUND, data1234), 0);
}

TEST_F(target_test, memory_write_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    // write while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->write_vmem(ADDR_IN_BOUND, data1234), mwr::report);
    sess.stop();
}

TEST_F(target_test, pmem_read_write) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    u64 in_bound_phys = targ->virt_to_phys(ADDR_IN_BOUND);
    u64 oo_bound_phys = targ->virt_to_phys(ADDR_OO_BOUND);

    // write in bounds
    EXPECT_NE(targ->write_pmem(in_bound_phys, data1234), 0);
    EXPECT_THAT(targ->read_pmem(in_bound_phys, 4), ElementsAre(1, 2, 3, 4));

    // write out of bounds
    EXPECT_EQ(targ->write_pmem(oo_bound_phys, data1234), 0);
    EXPECT_THAT(targ->read_pmem(oo_bound_phys, 4), ElementsAre(0, 0, 0, 0));

    EXPECT_NE(targ->write_pmem(0x0, inf_loop_inst), 0);

    // write while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->read_pmem(in_bound_phys, 4), mwr::report);
    EXPECT_THROW(targ->write_pmem(oo_bound_phys, data1234), mwr::report);
    sess.stop();
}

TEST_F(target_test, breakpoint) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    auto bp = targ->insert_breakpoint(0x04);
    EXPECT_EQ(bp.addr, 0x04);

    sess.run();
    ASSERT_TRUE(wait_for_target());

    EXPECT_EQ(targ->get_pc(), 4);
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_BREAKPOINT);

    targ->remove_breakpoint(bp);
}

TEST_F(target_test, breakpoint_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->insert_breakpoint(0x08), mwr::report);
    sess.stop();
}

TEST_F(target_test, watchpoint) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    auto wp_read = targ->insert_watchpoint(0x20, 0x04, WP_READ);
    EXPECT_EQ(wp_read.base, 0x20);
    EXPECT_EQ(wp_read.size, 0x04);

    auto wp_write = targ->insert_watchpoint(0x24, 0x04, WP_WRITE);
    EXPECT_EQ(wp_write.base, 0x24);
    EXPECT_EQ(wp_write.size, 0x04);

    vector<u8> inst_load{ 0x20, 0x01, 0x00, 0x11 };  // load from address 0x20
    vector<u8> inst_store{ 0x24, 0x01, 0x00, 0x10 }; // store to address 0x24
    EXPECT_EQ(targ->write_vmem(0x0, inst_load), 4);
    EXPECT_EQ(targ->write_vmem(0x4, inst_store), 4);

    sess.run();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);

    sess.run();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_RWATCHPOINT);
    targ->remove_watchpoint(wp_read);

    sess.run();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_WWATCHPOINT);
    targ->remove_watchpoint(wp_write);
}

TEST_F(target_test, watchpoint_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->insert_watchpoint(0x8, 0x04, WP_WRITE), mwr::report);
    sess.stop();
}

TEST_F(target_test, virt_to_phys) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_EQ(targ->virt_to_phys(0x04), 0x04);
}

TEST_F(target_test, virt_to_phys_while_running) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->virt_to_phys(0x04), mwr::report);
    sess.stop();
}

TEST_F(target_test, program_counter) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    // get pc while simulation is stopped
    EXPECT_EQ(targ->get_pc(), 0x0);

    // get pc while simulation is running
    sess.run();
    mwr::usleep(1000);
    EXPECT_THROW(targ->get_pc(), mwr::report);
    sess.stop();
}

TEST_F(target_test, stepping) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_EQ(targ->write_vmem(0x0, { 0x0, 0x0, 0x0, 0x0 }), 4);     // nop
    EXPECT_EQ(targ->write_vmem(0x4, { 0x0, 0x0, 0x0, 0x0 }), 4);     // nop
    EXPECT_EQ(targ->write_vmem(0x8, { 0x0, 0x0, 0x0, 0x0 }), 4);     // nop
    EXPECT_EQ(targ->write_vmem(0xc, { 0xf4, 0xff, 0xff, 0x20 }), 4); // back

    sess.stepi(*targ);
    ASSERT_TRUE(wait_for_target());

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_TARGET_STEP_COMPLETE);
    EXPECT_EQ(targ->get_pc(), 0x4);

    targ->step(3);
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(targ->get_pc(), 0x0);

    targ->step();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(targ->get_pc(), 0x4);

    // mocking a very slow simulation
    attribute* wait_per_inst = sess.find_attribute("system.cpu.wait_per_inst");
    ASSERT_NE(wait_per_inst, nullptr);
    wait_per_inst->set(500'000ull);
    targ->step(3);
    EXPECT_TRUE(sess.check_running());
    ASSERT_TRUE(wait_for_target());
    EXPECT_FALSE(sess.check_running());
    EXPECT_EQ(targ->get_pc(), 0x0);

    // only one step is performed because the simulation is very slow
    targ->step();
    targ->step();
    ASSERT_TRUE(wait_for_target());
    EXPECT_EQ(targ->get_pc(), 0x4);

    // step quantum blocking
    unsigned long long st_ns = sess.get_time_ns();
    unsigned long long quantum_ns = 2;
    sess.set_quantum(quantum_ns);
    EXPECT_EQ(sess.get_quantum_ns(), quantum_ns);
    sess.step(true);
    EXPECT_FALSE(sess.check_running());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_STEP_COMPLETE);
    EXPECT_EQ(sess.get_time_ns(), st_ns + quantum_ns);

    // step quantum non-blocking
    st_ns = sess.get_time_ns();
    sess.step(false);
    // EXPECT_TRUE(sess.running());
    ASSERT_TRUE(wait_for_target());
    EXPECT_FALSE(sess.check_running());
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_STEP_COMPLETE);
    EXPECT_EQ(sess.get_time_ns(), st_ns + quantum_ns);
}

TEST_F(target_test, stop_with_wait) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    sess.run();
    mwr::usleep(1000);
    sess.stop();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_USER);
}

TEST_F(target_test, stop_immediately) {
    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);

    EXPECT_NE(targ->write_vmem(0x0, inf_loop_inst), 0);

    sess.run();
    sess.stop();

    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_USER);
}

TEST_F(target_test, multi_session) {
    session sess2(HOST, PORT);

    target* targ = sess.find_target("system.cpu");
    ASSERT_NE(targ, nullptr);
    target* targ2 = sess2.find_target("system.cpu");
    ASSERT_NE(targ2, nullptr);

    u64 pc_before = targ->get_pc();
    EXPECT_EQ(pc_before, 0x0);

    u64 pc_before2 = targ->get_pc();
    EXPECT_EQ(pc_before2, 0x0);

    EXPECT_EQ(targ->write_vmem(0x0, { 0x0, 0x0, 0x0, 0x0 }), 4);     // nop
    EXPECT_EQ(targ->write_vmem(0x4, { 0x0, 0x0, 0x0, 0x0 }), 4);     // nop
    EXPECT_EQ(targ->write_vmem(0x8, { 0x0, 0x0, 0x0, 0x0 }), 4);     // nop
    EXPECT_EQ(targ->write_vmem(0xc, { 0xf4, 0xff, 0xff, 0x20 }), 4); // back

    auto async_step = std::async(std::launch::async,
                                 [&]() { targ2->step(6); });

    targ->step(6);

    async_step.get();

    ASSERT_TRUE(wait_for_target(sess));
    ASSERT_TRUE(wait_for_target(sess2));
    EXPECT_EQ(sess.reason().reason, VSP_STOP_REASON_TARGET_STEP_COMPLETE);
    EXPECT_EQ(sess2.reason().reason, VSP_STOP_REASON_TARGET_STEP_COMPLETE);

    u64 pc_after = targ->get_pc();
    EXPECT_EQ(pc_after, 0x8);

    u64 pc_after2 = targ->get_pc();
    EXPECT_EQ(pc_after2, 0x8);

    sess2.quit();
}
