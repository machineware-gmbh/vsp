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

target::target(connection& conn, const string& name):
    m_conn(conn), m_name(name) {
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

const string& target::name() const {
    return m_name;
}

void target::step(size_t steps) {
    for (size_t i = 0; i < steps; ++i)
        m_conn.command("step," + m_name);
}

mwr::u64 target::virt_to_phys(mwr::u64 va) {
    auto resp = m_conn.command("vapa," + m_name + "," + to_string(va));
    if (!connection::check_response(resp, 2))
        return 0;

    return stoull(resp->at(1), nullptr, 16);
}

u64 target::insert_breakpoint(u64 addr) {
    if (m_bp.count(addr))
        return m_bp.at(addr);

    auto resp = m_conn.command("mkbp," + m_name + "," + to_string(addr));
    if (!connection::check_response(resp, 2))
        return 0;

    u64 id = stoull(resp->at(1), nullptr, 16);
    m_bp[id] = addr;
    return id;
}

bool target::remove_breakpoint_id(u64 id) {
    bool found = false;
    for (auto& [a, i] : m_bp) {
        if (i == id) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    auto resp = m_conn.command("rmbp," + to_string(id));
    return connection::check_response(resp, 1);
}

bool target::remove_breakpoint(u64 addr) {
    if (!m_bp.count(addr))
        return false;

    return remove_breakpoint_id(m_bp.at(addr));
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
        ret.emplace_back(stoul(resp->at(i), nullptr, 16));

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
    for (auto& v : val)
        pc = (pc << 8) | v;

    return true;
}

const list<cpureg>& target::regs() const {
    return m_regs;
}

const cpureg* target::find_reg(const string& name) const {
    for (auto& reg : m_regs) {
        if (strcmp(reg.name(), name.c_str()) == 0)
            return &reg;
    }

    return nullptr;
}

} // namespace vsp
