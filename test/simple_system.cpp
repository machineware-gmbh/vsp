#include "simple_system.h"

#include <string>

static std::string reg_name(size_t ind) {
    static std::string names[33] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc"
    };

    return names[ind];
}

simple_cpu::simple_cpu(): vcml::processor("cpu", "riscv") {
    set_little_endian();

    // See: vcml/debugging/gdbarch.*
    define_cpureg_r(0, reg_name(0), sizeof(u32));
    for (int i = 1; i < reg_file.size(); ++i)
        define_cpureg_rw(i, reg_name(i), sizeof(u32));
    define_cpureg_rw(32, "pc", sizeof(u32));

    reset();
}

u64 simple_cpu::cycle_count() const {
    return cycle_counter;
};

u64 simple_cpu::program_counter() {
    return pc;
}

u64 simple_cpu::stack_pointer() {
    return reg_file[2];
}

u32 simple_cpu::fetch_inst(u32 adr) {
    u32 data;
    auto rs = insn.read(adr, &data, inst_size, vcml::SBI_NONE);

    if (rs == tlm::TLM_ADDRESS_ERROR_RESPONSE) {
        log_bus_error(insn, vcml::VCML_ACCESS_READ, rs, adr, inst_size);
        exit(1);
    } else if (rs != tlm::TLM_OK_RESPONSE) {
        log_bus_error(insn, vcml::VCML_ACCESS_READ, rs, adr, inst_size);
        exit(1);
    }

    return data;
}

void simple_cpu::reset() {
    pc = 0;

    for (auto& reg : reg_file) {
        reg = 0;
    }
}

#include <iostream>

void simple_cpu::simulate(size_t cycles) {
    for (int i = 0; i < cycles; ++i) {
        if (std::find(breakpoints_.begin(), breakpoints_.end(), pc) !=
            breakpoints_.end()) {
            notify_breakpoint_hit(pc, local_time_stamp());
            return;
        }

        u32 inst_data = fetch_inst(pc);
        pc = inst_data;
        retired_instructions++;
        cycle_counter++;
    }
    wait_clock_cycles(cycles);
};

bool simple_cpu::insert_breakpoint(vcml::u64 addr) {
    if (std::find(breakpoints_.begin(), breakpoints_.end(), addr) ==
        breakpoints_.end())
        breakpoints_.push_back(addr);
    return true;
}

bool simple_cpu::remove_breakpoint(vcml::u64 addr) {
    breakpoints_.erase(
        std::remove(breakpoints_.begin(), breakpoints_.end(), addr),
        breakpoints_.end());
    return true;
}

bool simple_cpu::read_reg_dbg(size_t regno, void* buf, size_t len) {
    if (regno < reg_file.size())
        *(u32*)buf = reg_file[regno];
    else if (regno == 32)
        *(u32*)buf = pc;
    else
        *(u32*)buf = 0;

    return true;
}

bool simple_cpu::write_reg_dbg(size_t regno, const void* buf, size_t len) {
    if (regno < reg_file.size())
        reg_file[regno] = *(u32*)buf;
    else if (regno == 32)
        reg_file[regno] = pc;

    return true;
}
