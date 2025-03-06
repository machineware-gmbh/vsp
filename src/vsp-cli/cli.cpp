/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp-cli/cli.h"

namespace vsp {

using mwr::termcolors;

cli::cli() {
    register_handler(&cli::handle_help, "help", "print this message", "h");
}

bool cli::handle_help(const string& args) {
    for (const auto& [cmd, handler] : m_handler) {
        string cmd_str = cmd;
        for (const auto& alias : m_alias) {
            if (alias.second == cmd)
                cmd_str += "|" + alias.first;
        }

        cout << termcolors::BOLD << termcolors::WHITE << std::left
             << std::setw(9) << cmd_str << termcolors::CLEAR << std::left
             << termcolors::WHITE << handler.second << termcolors::CLEAR
             << endl;
    }
    return true;
}

int cli::run() {
    while (print())
        ;
    return 0;
}

string cli::prompt() const {
    return "> ";
}

bool cli::before_run() {
    return true;
}

bool cli::print() {
    cout << endl;
    cout << prompt();

    string in;
    getline(cin, in);

    size_t blank_pos = in.find(' ');
    string cmd = in.substr(0, blank_pos);
    string args = blank_pos == string::npos
                      ? ""
                      : in.substr(blank_pos + 1, in.size() - blank_pos);

    auto it_alias = m_alias.find(cmd);
    if (it_alias != m_alias.end())
        cmd = it_alias->second;

    auto it = m_handler.find(cmd);
    if (it == m_handler.end())
        return true;

    return it->second.first(args);
}

} // namespace vsp
