/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/command.h"

#include "vsp/module.h"

namespace vsp {

command::command(const string& name, connection& conn, module* parent,
                 size_t argc, const string& desc):
    element(name, conn, parent), m_argc(argc), m_desc(desc) {
}

string command::execute(const vector<string>& args) {
    if (args.size() != m_argc) {
        return "need " + to_string(m_argc) + " arguments for " + name() +
               ", have " + to_string(args.size());
    }
    return execute(mwr::join(args, ','));
}

string command::execute(const string& args) {
    auto resp = m_conn.command("exec," + m_parent->hierarchy_name() + "," +
                               name() + (args.empty() ? "" : "," + args));
    if (!resp)
        throw std::runtime_error("error communicating with session");

    stringstream ss;
    for (size_t i = 1; i < resp->size(); ++i) {
        ss << resp.value()[i];
        if (i < resp->size() - 2)
            ss << ",";
    }

    if (resp.value()[0] == "E")
        throw std::runtime_error(ss.str());

    return ss.str();
}

const char* command::desc() const {
    return m_desc.c_str();
}

size_t command::argc() const {
    return m_argc;
}

} // namespace vsp
