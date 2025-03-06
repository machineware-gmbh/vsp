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

cpureg::cpureg(connection& conn, const string& name, target& parent):
    m_conn(conn), m_name(name), m_size(0), m_parent(parent) {
    update_size();
}

bool cpureg::update_size() {
    auto resp = m_conn.command("getr," + m_parent.name() + "," + m_name);
    if (!resp)
        return false;
    if (resp->at(0) != "OK")
        return false;
    m_size = resp->size() - 1;
    return true;
}

size_t cpureg::size() const {
    return m_size;
}

bool cpureg::get_value(vector<u8>& ret) {
    ret.clear();
    auto resp = m_conn.command("getr," + m_parent.name() + "," + m_name);
    if (!connection::check_response(resp, m_size + 1))
        return false;

    ret.reserve(resp->size() - 1);
    for (size_t i = 1; i < resp->size(); ++i)
        ret.emplace_back(static_cast<u8>(stoul(resp->at(i), nullptr, 16)));

    return true;
}

bool cpureg::set_value(const vector<u8>& ret) {
    if (ret.size() > m_size)
        return false;

    stringstream ss;
    ss << "setr," << m_parent.name() << ',' << m_name;
    for (auto& v : ret)
        ss << ',' << static_cast<u32>(v);

    auto resp = m_conn.command(ss.str());

    return connection::check_response(resp, 2);
}

const char* cpureg::name() const {
    return m_name.c_str();
}

} // namespace vsp
