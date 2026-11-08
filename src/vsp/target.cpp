/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsp/target.h"
#include "vsp/connection.h"

namespace vsp {

static string wp_type_str(watchpoint_type type) {
    switch (type) {
    case WP_READ:
        return "r";
    case WP_WRITE:
        return "w";
    case WP_ACCESS:
    default:
        return "rw";
    }
}

target* target_group::find_target(const string& name) const {
    for (auto* target : targets) {
        if (name == target->name())
            return target;
    }

    return nullptr;
}

target::target(connection& conn, const string& name, const string& arch,
               target_group& group):
    m_conn(conn), m_name(name), m_arch(arch), m_group(group), m_regs() {
    update_regs();

    if (m_arch.empty())
        fetch_arch();

    m_group.targets.push_back(this);
}

target::~target() {
    for (auto* reg : m_regs) {
        if (reg != nullptr)
            delete reg;
    }

    mwr::stl_remove(m_group.targets, this);
}

void target::update_regs() {
    auto resp = m_conn.command({ "lreg", m_name });
    for (size_t i = 1; i < resp.size(); ++i) {
        size_t regsize = 0;
        const string& regname = resp[i];
        size_t colon_pos = regname.find_last_of(':');
        if (colon_pos != string::npos)
            regsize = stoi(regname.substr(colon_pos + 1));

        auto reg = new cpureg(m_conn, regname.substr(0, colon_pos), *this,
                              regsize);
        m_regs.push_back(reg);
    }
}

void target::fetch_arch() {
    auto resp = m_conn.command({ "version" });
    int protover = resp.size() > 3 ? stoi(resp[3]) : 0;

    if (protover < 2) {
        resp = m_conn.command({ "geta", mkstr("%s.arch", m_name.c_str()) });
        MWR_REPORT_ON(resp.size() != 2, "%s: malformed response", __func__);
        m_arch = resp[1];
        return;
    }

    resp = m_conn.command({ "arch", m_name });
    MWR_REPORT_ON(resp.size() < 2, "malfomed arch response");
    m_arch = resp[1];
}

void target::step() {
    try {
        auto resp = m_conn.command({ "step", m_name });
    } catch (std::exception& ex) {
        string err = ex.what();
        MWR_REPORT_ON(err != "simulation running", "step failed");
    }
}

void target::step(size_t steps) {
    constexpr unsigned long long max_sleep = 1'000ull << 6;
    unsigned long long sleep = 1'000ull;

    for (size_t i = 0; i < steps; i++) {
        string err;
        do {
            err = "";

            try {
                auto resp = m_conn.command({ "step", m_name });
            } catch (std::exception& ex) {
                err = ex.what();
                MWR_REPORT_ON(err != "simulation running", "step failed");
                mwr::usleep(sleep);
                sleep *= sleep >= max_sleep ? 1 : 2;
            }
        } while (err == "simulation running");
    }
}

u64 target::virt_to_phys(u64 va) {
    auto resp = m_conn.command({ "vapa", m_name, to_string(va) });
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);
    return stoull(resp[1], nullptr, 16);
}

breakpoint target::insert_breakpoint(u64 addr) {
    auto resp = m_conn.command({ "mkbp", m_name, to_string(addr) });
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);
    const string& msg = resp[1];
    string bpstr = msg.substr(msg.find_last_of(' ') + 1);

    breakpoint bp;
    bp.addr = addr;
    bp.id = stoull(bpstr, nullptr, 10);
    return bp;
}

void target::remove_breakpoint(const breakpoint& bp) {
    m_conn.command({ "rmbp", to_string(bp.id) });
}

watchpoint target::insert_watchpoint(u64 base, u64 size,
                                     watchpoint_type type) {
    auto resp = m_conn.command({ "mkwp", m_name, to_string(base),
                                 to_string(size), wp_type_str(type) });
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);
    const string& msg = resp[1];
    string wpstr = msg.substr(msg.find_last_of(' ') + 1);

    watchpoint wp;
    wp.base = base;
    wp.size = size;
    wp.id = stoull(wpstr, nullptr, 10);
    wp.type = type;
    return wp;
}

void target::remove_watchpoint(const watchpoint& wp) {
    m_conn.command({ "rmwp", to_string(wp.id), wp_type_str(wp.type) });
}

vector<u8> target::read_vmem(u64 vaddr, size_t size) {
    vector<u8> ret;
    auto resp = m_conn.command(
        { "vread", m_name, to_string(vaddr), to_string(size) });
    if (resp.size() != size + 1)
        MWR_REPORT("%s: malformed response", __func__);

    ret.reserve(size);
    for (size_t i = 1; i < resp.size(); ++i)
        ret.emplace_back((u8)stoul(resp[i], nullptr, 16));
    return ret;
}

vector<u8> target::read_vmem_ex(u64 vaddr, size_t size,
                                vector<vmem_status>& status) {
    auto resp = m_conn.command(
        { "vread", m_name, to_string(vaddr), to_string(size), "status" });
    if (resp.size() != size + 1)
        MWR_REPORT("%s: malformed response", __func__);

    vector<u8> ret(size, 0);
    status.assign(size, VMEM_OK);
    for (size_t i = 0; i < size; ++i) {
        const string& tok = resp[i + 1];
        if (tok == "--")
            status[i] = VMEM_MMU_FAULT;
        else if (tok == "**")
            status[i] = VMEM_BUS_ERROR;
        else
            ret[i] = (u8)stoul(tok, nullptr, 16);
    }

    return ret;
}

static bool check_request(const vector<string>& resp, size_t n,
                          const char* func) {
    if (resp.size() == 1 && resp[0].empty())
        return false;
    if (resp.empty() || resp[0] != "OK")
        MWR_REPORT("%s", resp.size() > 1 ? resp[1].c_str() : "unknown error");
    if ((resp.size() - 1) % n != 0)
        MWR_REPORT("%s: malformed response", func);
    return true;
}

static u64 parse_hex(const string& s) {
    return s.empty() ? 0 : stoull(s, nullptr, 16);
}

vector<symbol> target::symbols() {
    vector<symbol> syms;
    auto resp = m_conn.request({ "lsym", m_name });

    if (!check_request(resp, 4, __func__))
        return syms;

    syms.reserve((resp.size() - 1) / 4);
    for (size_t i = 1; i + 3 < resp.size(); i += 4) {
        symbol sym;
        sym.is_code = resp[i] == "F";
        sym.addr = stoull(resp[i + 1], nullptr, 16);
        sym.size = stoull(resp[i + 2], nullptr, 10);
        sym.name = resp[i + 3];
        syms.push_back(std::move(sym));
    }

    return syms;
}

vector<instruction> target::disassemble(u64 vaddr, size_t count) {
    vector<instruction> insns;
    auto resp = m_conn.request(
        { "disas", m_name, to_string(vaddr), to_string(count) });

    if (!check_request(resp, 6, __func__))
        return insns;

    insns.reserve((resp.size() - 1) / 6);
    for (size_t i = 1; i + 5 < resp.size(); i += 6) {
        instruction insn;
        insn.virt = parse_hex(resp[i]);
        if (!resp[i + 1].empty())
            insn.phys = parse_hex(resp[i + 1]);
        for (size_t j = 0; j + 1 < resp[i + 2].size(); j += 2)
            insn.bytes.push_back((u8)parse_hex(resp[i + 2].substr(j, 2)));
        insn.symbol = resp[i + 3];
        insn.offset = parse_hex(resp[i + 4]);
        insn.code = resp[i + 5];
        insns.push_back(std::move(insn));
    }

    return insns;
}

vector<stackframe> target::stacktrace(size_t limit) {
    vector<stackframe> frames;
    auto resp = m_conn.request({ "stack", m_name, to_string(limit) });

    if (!check_request(resp, 4, __func__))
        return frames;

    frames.reserve((resp.size() - 1) / 4);
    for (size_t i = 1; i + 3 < resp.size(); i += 4) {
        frames.push_back({ parse_hex(resp[i]), parse_hex(resp[i + 1]),
                           resp[i + 2], parse_hex(resp[i + 3]) });
    }

    return frames;
}

target_info target::info() {
    target_info info{};
    auto resp = m_conn.command({ "tinfo", m_name });
    for (size_t i = 1; i < resp.size(); i++) {
        size_t sep = resp[i].find(':');
        if (sep == string::npos)
            continue;

        string key = resp[i].substr(0, sep);
        u64 val = parse_hex(resp[i].substr(sep + 1));
        if (key == "id")
            info.id = val;
        else if (key == "pc")
            info.pc = val;
        else if (key == "lr")
            info.lr = val;
        else if (key == "sp")
            info.sp = val;
        else if (key == "fp")
            info.fp = val;
    }

    return info;
}

size_t target::write_vmem(u64 vaddr, const vector<u8>& data) {
    vector<string> cmd = { "vwrite", m_name, to_string(vaddr) };
    cmd.reserve(3 + data.size());
    for (u8 v : data)
        cmd.push_back(to_string((int)v));

    auto resp = m_conn.command(cmd);
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);

    auto parts = split(resp[1], ' ');
    if (parts.size() != 3)
        return 0;

    const string& bytes_written = parts[0];
    return stoull(bytes_written, nullptr, 10);
}

vector<u8> target::read_pmem(u64 paddr, size_t size) {
    vector<u8> ret;
    auto resp = m_conn.command(
        { "pread", m_name, to_string(paddr), to_string(size) });
    if (resp.size() != size + 1)
        MWR_REPORT("%s malformed response", __func__);

    ret.reserve(size);
    for (size_t i = 1; i < resp.size(); ++i)
        ret.emplace_back((u8)stoul(resp[i], nullptr, 16));

    return ret;
}

size_t target::write_pmem(u64 paddr, const vector<u8>& data) {
    vector<string> cmd = { "pwrite", m_name, to_string(paddr) };
    cmd.reserve(3 + data.size());
    for (u8 v : data)
        cmd.push_back(to_string((int)v));

    auto resp = m_conn.command(cmd);
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);

    auto parts = split(resp[1], ' ');
    if (parts.size() != 3)
        return 0;

    const string& bytes_written = parts[0];
    return stoull(bytes_written, nullptr, 10);
}

u64 target::get_pc() {
    cpureg* pc_reg = find_reg("pc");
    if (!pc_reg)
        pc_reg = find_reg("PC");
    MWR_REPORT_ON(!pc_reg, "cannot find program counter");

    vector<u8> val;
    pc_reg->get_value(val);

    u64 pc = 0;
    for (auto it = val.rbegin(); it != val.rend(); ++it)
        pc = (pc << 8) | *it;

    return pc;
}

cpureg* target::find_reg(const string& name) {
    for (auto& reg : m_regs) {
        if (strcmp(reg->name(), name.c_str()) == 0)
            return reg;
    }

    return nullptr;
}

} // namespace vsp
