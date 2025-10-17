#include "simple_cpu.h"

static string reg_name(size_t ind) {
    static string names[33] = { "zero", "ra", "sp", "gp", "tp", "t0",  "t1",
                                "t2",   "s0", "s1", "a0", "a1", "a2",  "a3",
                                "a4",   "a5", "a6", "a7", "s2", "s3",  "s4",
                                "s5",   "s6", "s7", "s8", "s9", "s10", "s11",
                                "t3",   "t4", "t5", "t6", "pc" };

    return names[ind];
}

simple_cpu::simple_cpu(const char* name): vcml::processor(name, "riscv") {
    set_little_endian();

    define_cpureg_r(0, reg_name(0), sizeof(u32));
    for (size_t i = 1; i < reg_file.size(); ++i)
        define_cpureg_rw(i, reg_name(i), sizeof(u32));
    define_cpureg_rw(32, "pc", sizeof(u32));

    reset();
}

u64 simple_cpu::cycle_count() const {
    return m_num_cycles;
};

u64 simple_cpu::program_counter() {
    return pc;
}

u64 simple_cpu::stack_pointer() {
    return reg_file[2];
}

void simple_cpu::reset() {
    pc = 0;

    for (auto& reg : reg_file)
        reg = 0;
}

void simple_cpu::simulate(size_t cycles) {
    for (size_t i = 0; i < cycles; i++) {
        if (std::find(m_breakpoints.begin(), m_breakpoints.end(), pc) !=
            m_breakpoints.end()) {
            notify_breakpoint_hit(pc, local_time_stamp());
            return;
        }

        exec_inst(mem_read(pc));

        m_num_retired_insts++;
        m_num_cycles++;
    }

    wait_clock_cycles(cycles);
};

bool simple_cpu::insert_breakpoint(u64 addr) {
    if (std::find(m_breakpoints.begin(), m_breakpoints.end(), addr) ==
        m_breakpoints.end())
        m_breakpoints.push_back(addr);
    return true;
}

bool simple_cpu::remove_breakpoint(u64 addr) {
    m_breakpoints.erase(
        std::remove(m_breakpoints.begin(), m_breakpoints.end(), addr),
        m_breakpoints.end());
    return true;
}

bool simple_cpu::insert_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) {
    for (auto& wp : m_watchpoints) {
        if (std::get<0>(wp) == addr && std::get<1>(wp) == prot)
            return false;
    }

    m_watchpoints.emplace_back(addr, prot);
    return true;
}

bool simple_cpu::remove_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) {
    m_watchpoints.erase(
        std::remove_if(
            m_watchpoints.begin(), m_watchpoints.end(),
            [&](const std::tuple<vcml::range, vcml::vcml_access>& wp) {
                return std::get<0>(wp) == addr && std::get<1>(wp) == prot;
            }),
        m_watchpoints.end());
    return true;
}

bool simple_cpu::virt_to_phys(u64 vaddr, u64& paddr) {
    paddr = vaddr;
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
        pc = *(u32*)buf;

    return true;
}

void simple_cpu::exec_inst(u32 inst) {
    // simple ISA:
    //  - opcode in bits [31:24]
    //  - store (0x10): [23:16]=base, [15:8]=src, [7:0]=offset8 (signed)
    //  - load  (0x11): [23:16]=base, [15:8]=dst, [7:0]=offset8 (signed)
    //  - rjump (0x20): [23:0]=signed24 relative offset
    //  - default: nop (advance pc)

    u8 opcode = (inst >> 24) & 0xff;

    switch (opcode) {
    case 0x10: { // store
        u8 base = (inst >> 16) & 0x1f;
        u8 src = (inst >> 8) & 0x1f;
        i8 off = (i8)(inst & 0xff);
        u64 addr = (u64)reg_file[base] + (i64)off;
        u32 data = reg_file[src];
        mem_write(addr, data);
        pc += inst_size;
        break;
    }

    case 0x11: { // load
        u8 base = (inst >> 16) & 0x1f;
        u8 dst = (inst >> 8) & 0x1f;
        i8 off = (i8)(inst & 0xff);
        u64 addr = (u64)(reg_file[base]) + (i64)(off);
        u32 val = mem_read(addr, true);
        reg_file[dst] = val;
        pc += inst_size;
        break;
    }

    case 0x20: { // relative jump
        i32 rel = inst & 0x00ffffff;
        if (rel & 0x00800000)
            rel |= 0xff000000;
        pc = (u32)((i32)pc + rel);
        break;
    }

    default:
        pc += inst_size;
        break;
    }
}

u32 simple_cpu::mem_read(u32 adr, bool trigger_watchpoints) {
    u32 dat;
    auto rs = data.read(adr, &dat, sizeof(u32), vcml::SBI_NONE);

    if (rs != tlm::TLM_OK_RESPONSE) {
        log_bus_error(insn, vcml::VCML_ACCESS_READ, rs, adr, sizeof(u32));
    }

    if (trigger_watchpoints) {
        for (auto& wp : m_watchpoints) {
            if (std::get<0>(wp).includes(adr) &&
                std::get<1>(wp) == vcml::vcml_access::VCML_ACCESS_READ) {
                notify_watchpoint_read(std::get<0>(wp), local_time_stamp());
            }
        }
    }

    return dat;
}

void simple_cpu::mem_write(u32 adr, u32 dat) {
    void* d = &dat;
    auto rs = data.write(adr, d, sizeof(u32), vcml::SBI_NONE);

    if (rs != tlm::TLM_OK_RESPONSE) {
        log_bus_error(insn, vcml::VCML_ACCESS_WRITE, rs, adr, sizeof(u32));
    }

    for (auto& wp : m_watchpoints) {
        if (std::get<0>(wp).includes(adr) &&
            std::get<1>(wp) == vcml::vcml_access::VCML_ACCESS_WRITE) {
            notify_watchpoint_write(std::get<0>(wp), d, local_time_stamp());
        }
    }
}
