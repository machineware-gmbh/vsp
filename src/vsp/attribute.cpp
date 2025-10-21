/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/attribute.h"

namespace vsp {

attribute::attribute(const string& name, connection& conn, module* parent,
                     const string& type, size_t count):
    element(name, conn, parent), m_type(type), m_count(count) {
}

const string& attribute::type() const {
    return m_type;
}

size_t attribute::count() const {
    return m_count;
}

optional<vector<string>> attribute::get() {
    if (m_count == 0)
        return nullopt;

    optional<vector<string>> resp = m_conn.command("geta," + hierarchy_name());

    if (!connection::check_response(resp, m_count + 1))
        return nullopt;

    resp->erase(resp->begin());
    return std::move(resp.value());
}

string attribute::get_str() {
    auto val = get();
    if (!val)
        return "<error>";

    stringstream ss;
    for (size_t i = 0; i < val.value().size(); ++i) {
        ss << val.value()[i];
        if (i != val.value().size() - 1)
            ss << ",";
    }
    return ss.str();
}

bool attribute::set(const string& val) {
    auto resp = m_conn.command("seta," + hierarchy_name() + "," + val);
    return connection::check_response(resp, 1);
}

bool attribute::set(bool val) {
    return set(string(val ? "true" : "false"));
}

bool attribute::set(int val) {
    return set(to_string(val));
}

bool attribute::set(long val) {
    return set(to_string(val));
}

bool attribute::set(long long val) {
    return set(to_string(val));
}

bool attribute::set(unsigned val) {
    return set(to_string(val));
}

bool attribute::set(unsigned long val) {
    return set(to_string(val));
}

bool attribute::set(unsigned long long val) {
    return set(to_string(val));
}

bool attribute::set(float val) {
    return set(to_string(val));
}

bool attribute::set(double val) {
    return set(to_string(val));
}

bool attribute::set(long double val) {
    return set(to_string(val));
}

} // namespace vsp
