#include "simple_cpu.h"

static std::string reg_name(size_t ind) {
    const static std::string names[33] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc"
    };

    return names[ind];
}

simple_cpu::simple_cpu(const char* name):
    vcml::processor(name, "riscv"),
    wait_per_inst("wait_per_inst", 0),
    bool_property("bool_property", false),
    i32_property("i32_property", 0),
    i64_property("i64_property", 0),
    u32_property("u32_property", 0),
    u64_property("u64_property", 0),
    float_property("float_property", 0.f),
    double_property("double_property", 0.),
    long_double_property("long_double_property", 0.l),
    string_vector_property("string_vector_property", { "", "", "" }),
    i32_vector_property("i32_vector_property", { 0, 0, 0 }) {
    set_little_endian();

    define_cpureg_r(0, reg_name(0), sizeof(reg_t));
    for (size_t i = 1; i < m_reg_file.size(); ++i)
        define_cpureg_rw(i, reg_name(i), sizeof(reg_t));
    define_cpureg_rw(32, "pc", sizeof(reg_t));

    reset();
}

mwr::u64 simple_cpu::cycle_count() const {
    return m_num_cycles;
};

mwr::u64 simple_cpu::program_counter() {
    return m_pc;
}

mwr::u64 simple_cpu::stack_pointer() {
    return m_reg_file[2];
}

void simple_cpu::reset() {
    mwr::log_debug("resetting cpu");
    m_pc = 0;

    for (auto& reg : m_reg_file)
        reg = 0;
}

void simple_cpu::simulate(size_t cycles) {
    mwr::log_debug("simulating %lu cycle(s)", cycles);

    if (wait_per_inst.get() != 0)
        mwr::usleep(wait_per_inst.get());

    for (size_t i = 0; i < cycles; i++) {
        m_num_cycles++;

        try {
            exec_inst();
        } catch (cpu_exception exception) {
            return;
        }
    }
};

bool simple_cpu::insert_breakpoint(u64 addr) {
    if (std::find(m_breakpoints.begin(), m_breakpoints.end(), addr) ==
        m_breakpoints.end())
        m_breakpoints.push_back(addr);
    mwr::log_debug("breakpoint inserted at addr=0x%08x", (u32)addr);
    return true;
}

bool simple_cpu::remove_breakpoint(u64 addr) {
    m_breakpoints.erase(
        std::remove(m_breakpoints.begin(), m_breakpoints.end(), addr),
        m_breakpoints.end());
    mwr::log_debug("breakpoint removed at addr=0x%08x", (u32)addr);
    return true;
}

bool simple_cpu::insert_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) {
    for (auto& wp : m_watchpoints) {
        if (std::get<0>(wp) == addr && std::get<1>(wp) == prot)
            return false;
    }

    m_watchpoints.emplace_back(addr, prot);
    mwr::log_debug("watchpoint inserted at 0x%08x-0x%08x", (u32)addr.start,
                   (u32)addr.end);
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
    mwr::log_debug("watchpoint removed at 0x%08x-0x%08x", (u32)addr.start,
                   (u32)addr.end);
    return true;
}

bool simple_cpu::virt_to_phys(u64 vaddr, u64& paddr) {
    paddr = vaddr;
    return true;
}

bool simple_cpu::read_reg_dbg(size_t regno, void* buf, size_t len) {
    mwr::log_debug("debug read of register %lu", regno);
    if (regno < m_reg_file.size())
        *(reg_t*)buf = m_reg_file[regno];
    else if (regno == 32)
        *(reg_t*)buf = m_pc;
    else
        *(reg_t*)buf = 0;

    return true;
}

bool simple_cpu::write_reg_dbg(size_t regno, const void* buf, size_t len) {
    mwr::log_debug("debug write of register %lu", regno);
    if (regno < m_reg_file.size())
        m_reg_file[regno] = *(reg_t*)buf;
    else if (regno == 32)
        m_pc = *(reg_t*)buf;

    return true;
}

void simple_cpu::exec_inst() {
    mwr::log_debug("fetching instruction at pc=0x%08x", m_pc);

    if (std::find(m_breakpoints.begin(), m_breakpoints.end(), m_pc) !=
        m_breakpoints.end()) {
        notify_breakpoint_hit(m_pc, local_time_stamp());
        mwr::log_debug("breakpoint hit at pc=0x%08x", m_pc);
        throw cpu_exception(HIT_BREAKPOINT);
    }

    inst_t inst = mem_read(m_pc);
    u8 opcode = (inst >> 24) & 0xff;

    mwr::log_debug("executing opcode 0x%02x", opcode);

    // simple ISA:
    //  - opcode in bits [31:24]
    //  - store (0x10): [23:16]=base, [15:8]=src, [7:0]=offset8 (signed)
    //  - load  (0x11): [23:16]=base, [15:8]=dst, [7:0]=offset8 (signed)
    //  - rjump (0x20): [23:0]=signed24 relative offset
    //  - default: nop
    switch (opcode) {
    case 0x10: { // store
        u8 base = (inst >> 16) & 0x1f;
        u8 src = (inst >> 8) & 0x1f;
        i8 off = (i8)(inst & 0xff);
        u64 addr = (u64)m_reg_file[base] + (i64)off;
        reg_t data = m_reg_file[src];
        mem_write(addr, data);
        m_pc += sizeof(inst_t);
        break;
    }

    case 0x11: { // load
        u8 base = (inst >> 16) & 0x1f;
        u8 dst = (inst >> 8) & 0x1f;
        i8 off = (i8)(inst & 0xff);
        u64 addr = (u64)(m_reg_file[base]) + (i64)(off);
        u32 val = mem_read(addr, true);
        m_reg_file[dst] = val;
        m_pc += sizeof(inst_t);
        break;
    }

    case 0x20: { // relative jump
        i32 rel = inst & 0x00ffffff;
        if (rel & 0x00800000)
            rel |= 0xff000000;
        m_pc = (reg_t)((i32)m_pc + rel);
        break;
    }

    default:
        m_pc += sizeof(inst_t);
        break;
    }
}

mwr::u32 simple_cpu::mem_read(u32 adr, bool trigger_watchpoints) {
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
                mwr::log_debug("read watchpoint hit at pc=0x%08x", m_pc);
                throw cpu_exception(HIT_RWATCHPOINT);
            }
        }
    }

    return dat;
}

void simple_cpu::mem_write(u32 adr, reg_t dat) {
    void* d = &dat;
    auto rs = data.write(adr, d, sizeof(reg_t), vcml::SBI_NONE);

    if (rs != tlm::TLM_OK_RESPONSE) {
        log_bus_error(insn, vcml::VCML_ACCESS_WRITE, rs, adr, sizeof(reg_t));
    }

    for (auto& wp : m_watchpoints) {
        if (std::get<0>(wp).includes(adr) &&
            std::get<1>(wp) == vcml::vcml_access::VCML_ACCESS_WRITE) {
            notify_watchpoint_write(std::get<0>(wp), d, local_time_stamp());
            mwr::log_debug("write watchpoint hit at pc=0x%08x", m_pc);
            throw cpu_exception(HIT_WWATCHPOINT);
        }
    }
}
