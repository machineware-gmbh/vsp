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

vector<string> attribute::get() {
    if (m_count == 0)
        return vector<string>();

    auto resp = m_conn.command("geta," + hierarchy_name());
    MWR_REPORT_ON(resp.size() != 2, "%s: malformed response", __func__);
    resp.erase(resp.begin());
    return resp;
}

string attribute::get_str() {
    try {
        auto val = get();

        stringstream ss;
        for (size_t i = 0; i < val.size(); ++i) {
            ss << val[i];
            if (i != val.size() - 1)
                ss << ",";
        }

        return ss.str();

    } catch (...) {
        return "<error>";
    }
}

void attribute::set_escaped(const string& val) {
    m_conn.command("seta," + hierarchy_name() + "," + val);
}

void attribute::set(const char* val) {
    set(string(val));
}

void attribute::set(const string& val) {
    set_escaped(mwr::escape(val, ","));
}

void attribute::set(bool val) {
    set_escaped(string(val ? "true" : "false"));
}

} // namespace vsp
