/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp-cli/startup_cli.h"

#include "vsp-cli/session_cli.h"

namespace vsp {

using mwr::termcolors;

startup_cli::startup_cli(): m_session(nullptr) {
    auto& local_sessions = session::get_sessions();
    m_sessions.insert(m_sessions.end(), local_sessions.begin(),
                      local_sessions.end());

    register_handler(&startup_cli::handle_exit, "exit", "exit the program",
                     "e");
    register_handler(&startup_cli::handle_list, "list",
                     "list available sessions", "l");
    register_handler(&startup_cli::handle_select, "select",
                     "connect to an available session by id", "s");
    register_handler(&startup_cli::handle_connect, "connect",
                     "connect to a session using <host>:<port>", "c");
}

bool startup_cli::handle_exit(const string& args) {
    cout << "exiting..." << endl;
    return false;
}

bool startup_cli::handle_list(const string& args) {
    if (m_sessions.empty()) {
        cout << "no sessions available" << endl;
        return true;
    }

    size_t id = 0;
    for (const auto& s : m_sessions) {
        cout << id++ << ": "
             << (s->is_connected()
                     ? string(termcolors::YELLOW) + string(termcolors::BOLD)
                     : string())
             << s->host() << ":" << s->port() << termcolors::CLEAR << endl;
    }
    return true;
}

bool startup_cli::handle_select(const string& args) {
    size_t id;
    try {
        id = stoul(args);
    } catch (...) {
        cout << "invalid session id" << endl;
        return true;
    }

    if (id >= m_sessions.size()) {
        cout << "invalid session id" << endl;
        return true;
    }

    m_session = m_sessions[id];
    session_cli();
    return true;
}

bool startup_cli::handle_connect(const string& args) {
    vector<string> parts = split(args, ':');
    if (parts.size() != 2) {
        cout << "invalid server format - use <server>:<port>" << endl;
        return true;
    }
    string host = parts[0];
    u16 port;

    try {
        port = stoul(parts[1]);
    } catch (...) {
        cout << "invalid port number" << endl;
        return true;
    }

    for (const auto& s : m_sessions) {
        if (s->host() == host && s->port() == port) {
            m_session = s;
            session_cli();
            return true;
        }
    }
    auto s = make_shared<session>(host, port);
    m_sessions.push_back(s);
    m_session = std::move(s);
    session_cli();

    return true;
}

void startup_cli::session_cli() {
    class session_cli cli(m_session);
    cli.run();
}

} // namespace vsp
