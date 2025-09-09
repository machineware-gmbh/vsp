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

static const char* wp_type_str(watchpoint_type type) {
    return (type == WP_READ) ? "r" : (type == WP_WRITE) ? "w" : "rw";
}

target::target(connection& conn, const string& name):
    m_conn(conn), m_name(name), m_regs() {
    update_regs();
    vector<u8> data(4, 0);
    m_regs.front().get_value(data);
}

bool target::update_regs() {
    auto resp = m_conn.command("lreg," + m_name);
    if (!resp)
        return false;

    if (resp->at(0) != "OK")
        return false;

    for (size_t i = 1; i < resp->size(); ++i)
        m_regs.emplace_back(m_conn, resp->at(i), *this);

    return true;
}

const char* target::name() const {
    return m_name.c_str();
}

void target::step(size_t steps) {
    for (size_t i = 0; i < steps; ++i)
        m_conn.command("step," + m_name);
}

u64 target::virt_to_phys(u64 va) {
    auto resp = m_conn.command("vapa," + m_name + "," + to_string(va));
    if (!connection::check_response(resp, 2))
        return 0;

    return stoull(resp->at(1), nullptr, 16);
}

optional<breakpoint> target::insert_breakpoint(u64 addr) {
    auto resp = m_conn.command("mkbp," + m_name + "," + to_string(addr));
    if (!connection::check_response(resp, 2))
        return nullopt;

    const string& msg = resp->at(1);
    string bpstr = msg.substr(msg.find_last_of(' ') + 1);

    breakpoint bp;
    bp.addr = addr;
    bp.id = stoull(bpstr, nullptr, 10);
    return bp;
}

bool target::remove_breakpoint(const breakpoint& bp) {
    auto resp = m_conn.command("rmbp," + to_string(bp.id));
    return connection::check_response(resp, 1);
}

optional<watchpoint> target::insert_watchpoint(u64 base, u64 size,
                                               watchpoint_type type) {
    auto resp = m_conn.command("mkwp," + m_name + "," + to_string(base) + "," +
                               to_string(size) + "," +
                               string(wp_type_str(type)));
    if (!connection::check_response(resp, 2))
        return nullopt;

    const string& msg = resp->at(1);
    string wpstr = msg.substr(msg.find_last_of(' ') + 1);

    watchpoint wp;
    wp.base = base;
    wp.size = size;
    wp.id = stoull(wpstr, nullptr, 10);
    wp.type = type;
    return wp;
}

bool target::remove_watchpoint(const watchpoint& wp) {
    auto resp = m_conn.command("rmwp," + to_string(wp.id) + "," +
                               string(wp_type_str(wp.type)));
    return connection::check_response(resp, 1);
}

vector<u8> target::read_vmem(u64 vaddr, size_t size) {
    vector<u8> ret;
    string cmd = "vread," + m_name + "," + to_string(vaddr) + ',' +
                 to_string(size);
    auto resp = m_conn.command(cmd);
    if (!connection::check_response(resp, size + 1))
        return ret;

    ret.reserve(size);
    for (size_t i = 1; i < resp->size(); ++i)
        ret.emplace_back((u8)stoul(resp->at(i), nullptr, 16));

    return ret;
}

bool target::write_vmem(u64 vaddr, const vector<u8>& data) {
    stringstream ss;
    ss << "vwrite," << m_name << ',' << vaddr;
    for (auto& v : data)
        ss << ',' << static_cast<u32>(v);

    auto resp = m_conn.command(ss.str());
    return connection::check_response(resp, 2);
}

bool target::pc(u64& pc) {
    cpureg* pc_reg = nullptr;

    for (auto& reg : m_regs) {
        if (strcmp(reg.name(), "pc") == 0 || strcmp(reg.name(), "PC") == 0) {
            pc_reg = &reg;
            break;
        }
    }

    if (!pc_reg)
        return false;

    vector<u8> val;
    if (!pc_reg->get_value(val))
        return false;

    pc = 0;
    for (auto it = val.rbegin(); it != val.rend(); ++it)
        pc = (pc << 8) | *it;

    return true;
}

list<cpureg>& target::regs() {
    return m_regs;
}

cpureg* target::find_reg(const string& name) {
    for (auto& reg : m_regs) {
        if (strcmp(reg.name(), name.c_str()) == 0)
            return &reg;
    }

    return nullptr;
}

} // namespace vsp
