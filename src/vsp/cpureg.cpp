/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/cpureg.h"
#include "vsp/target.h"

namespace vsp {

cpureg::cpureg(connection& conn, const string& name, target& parent,
               size_t size):
    m_conn(conn), m_name(name), m_size(size), m_parent(parent) {
    if (m_size == 0)
        update_size();
}

void cpureg::update_size() {
    auto resp = m_conn.command("getr," + string(m_parent.name()) + "," +
                               m_name);
    m_size = resp.size() - 1;
}

size_t cpureg::size() const {
    return m_size;
}

void cpureg::get_value(vector<u8>& ret) {
    ret.clear();
    auto resp = m_conn.command("getr," + string(m_parent.name()) + "," +
                               m_name);
    if (resp.size() != m_size + 1)
        MWR_REPORT("%s: malformed response", __func__);

    ret.reserve(resp.size() - 1);
    for (size_t i = 1; i < resp.size(); ++i)
        ret.emplace_back((u8)(stoul(resp.at(i), nullptr, 16)));
}

void cpureg::set_value(const vector<u8>& val) {
    if (val.size() != m_size)
        MWR_REPORT("%s: invalid initializer", __func__);

    stringstream ss;
    ss << "setr," << m_parent.name() << ',' << m_name;
    for (auto& v : val)
        ss << ',' << static_cast<u32>(v);

    m_conn.command(ss.str());
}

const char* cpureg::name() const {
    return m_name.c_str();
}

} // namespace vsp
