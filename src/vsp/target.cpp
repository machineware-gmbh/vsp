/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/target.h"

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

target::target(connection& conn, const string& name):
    m_conn(conn), m_name(name), m_regs() {
    update_regs();
}

void target::update_regs() {
    auto resp = m_conn.command("lreg," + m_name);
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

const char* target::name() const {
    return m_name.c_str();
}

void target::step() {
    try {
        auto resp = m_conn.command("step," + m_name);
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
                auto resp = m_conn.command("step," + m_name);
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
    auto resp = m_conn.command("vapa," + m_name + "," + to_string(va));
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);
    return stoull(resp[1], nullptr, 16);
}

breakpoint target::insert_breakpoint(u64 addr) {
    auto resp = m_conn.command("mkbp," + m_name + "," + to_string(addr));
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);
    const string& msg = resp[1];
    string bpstr = msg.substr(msg.find_last_of(' ') + 1);

    breakpoint bp;
    bp.addr = addr;
    bp.id = stoull(bpstr, nullptr, 10);
    return bp;
}

void target::remove_breakpoint(const breakpoint& bp) {
    m_conn.command("rmbp," + to_string(bp.id));
}

watchpoint target::insert_watchpoint(u64 base, u64 size,
                                     watchpoint_type type) {
    auto resp = m_conn.command("mkwp," + m_name + "," + to_string(base) + "," +
                               to_string(size) + "," + wp_type_str(type));
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
    m_conn.command("rmwp," + to_string(wp.id) + "," + wp_type_str(wp.type));
}

vector<u8> target::read_vmem(u64 vaddr, size_t size) {
    vector<u8> ret;
    string cmd = "vread," + m_name + "," + to_string(vaddr) + ',' +
                 to_string(size);
    auto resp = m_conn.command(cmd);
    if (resp.size() != size + 1)
        MWR_REPORT("%s: malformed response", __func__);

    ret.reserve(size);
    for (size_t i = 1; i < resp.size(); ++i)
        ret.emplace_back((u8)stoul(resp[i], nullptr, 16));
    return ret;
}

size_t target::write_vmem(u64 vaddr, const vector<u8>& data) {
    stringstream ss;
    ss << "vwrite," << m_name << ',' << vaddr;
    for (auto& v : data)
        ss << ',' << static_cast<u32>(v);

    auto resp = m_conn.command(ss.str());
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);

    auto parts = split(resp[1], ' ');
    if (parts.size() != 3)
        return 0;

    string bytes_written = parts[0];
    return stoull(bytes_written, nullptr, 10);
}

vector<u8> target::read_pmem(u64 paddr, size_t size) {
    vector<u8> ret;
    string cmd = "pread," + m_name + "," + to_string(paddr) + ',' +
                 to_string(size);
    auto resp = m_conn.command(cmd);
    if (resp.size() != size + 1)
        MWR_REPORT("%s malformed response", __func__);

    ret.reserve(size);
    for (size_t i = 1; i < resp.size(); ++i)
        ret.emplace_back((u8)stoul(resp[i], nullptr, 16));

    return ret;
}

size_t target::write_pmem(u64 paddr, const vector<u8>& data) {
    stringstream ss;
    ss << "pwrite," << m_name << ',' << paddr;
    for (auto& v : data)
        ss << ',' << static_cast<u32>(v);

    auto resp = m_conn.command(ss.str());
    MWR_REPORT_ON(resp.size() < 2, "%s: malformed response", __func__);

    auto parts = split(resp[1], ' ');
    if (parts.size() != 3)
        return 0;

    string bytes_written = parts[0];
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
