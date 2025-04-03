/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "cli/session.h"

#include "vsp/version.h"

namespace cli {

using mwr::termcolors;

session::session(shared_ptr<vsp::session> s):
    m_session(std::move(s)), m_current_mod(nullptr) {
    if (!m_session->is_connected())
        m_session->connect();
    m_current_mod = m_session->find_module("");

    register_handler(&session::handle_cd, "cd",
                     "moves current module to <module>");
    register_handler(&session::handle_exec, "exec",
                     "executes the given <command> [args...]", "x");
    register_handler(&session::handle_info, "info",
                     "print information about the current session", "i");
    register_handler(&session::handle_kill, "kill",
                     "terminate current session", "k");
    register_handler(
        &session::handle_list, "list",
        "displays the module hierarchy onwards from current module", "ls");
    register_handler(&session::handle_quit, "quit", "disconnect from session",
                     "q");
    register_handler(&session::handle_detach, "detach", "deatch from session",
                     "d");
    register_handler(&session::handle_read, "read",
                     "reads the given <attribute>", "r");
    register_handler(&session::handle_run, "run", "continues simulation", "c");
    register_handler(&session::handle_stop, "stop", "stops the simulation");
    register_handler(&session::handle_step, "step",
                     "advances simulation to the next discrete timestamp",
                     "s");
}

bool session::handle_list(const string& args) {
    bool show_mods = args.find("-m") != string::npos;
    bool show_attr = args.find("-a") != string::npos;
    bool show_cmd = args.find("-c") != string::npos;

    if (!show_mods && !show_attr && !show_cmd) {
        show_mods = true;
        show_attr = true;
        show_cmd = true;
    }

    if (show_mods) {
        for (auto& m : m_current_mod->get_modules()) {
            cout << termcolors::BOLD << termcolors::CYAN << m->name()
                 << termcolors::CLEAR << endl;
        }
    }

    if (show_attr) {
        for (auto& a : m_current_mod->get_attritbutes()) {
            cout << termcolors::WHITE << a->name() << termcolors::CLEAR
                 << endl;
        }
    }

    if (show_cmd) {
        vsp::command* cinfo = m_current_mod->find_command("cinfo");
        for (auto& c : m_current_mod->get_commands()) {
            cout << termcolors::BOLD << termcolors::MAGENTA << c->name()
                 << termcolors::CLEAR;

            if (cinfo)
                cout << " " << cinfo->execute(c->name());
            cout << endl;
        }
    }
    return true;
}

bool session::handle_cd(const string& args) {
    if (args == "..") {
        if (!m_current_mod->parent()) {
            cout << "current module has no parent" << endl;
            return true;
        }
        m_current_mod = m_current_mod->parent();
        return true;
    }

    if (args == "") {
        m_current_mod = m_session->find_module();
        return true;
    }

    vsp::module* m = m_current_mod->find_module(args);
    if (!m) {
        cout << "module '" << args << "' does not exist!" << endl;
        return true;
    }

    m_current_mod = m;
    return true;
}

bool session::handle_read(const string& args) {
    vsp::attribute* a = m_current_mod->find_attribute(args);
    if (!a) {
        cout << "attribute '" << args << "' does not exist!" << endl;
        return true;
    }

    cout << termcolors::BOLD << termcolors::WHITE << a->name()
         << termcolors::CLEAR << " " << a->get_str() << endl;
    return true;
}

bool session::handle_step(const string& args) {
    m_session->step();
    return true;
}

void session::print_report_line(const string& kind, const string& value) {
    cout << termcolors::BOLD << termcolors::WHITE << std::left << std::setw(16)
         << kind << termcolors::CLEAR << std::left << termcolors::WHITE
         << value << termcolors::CLEAR << endl;
}

bool session::handle_info(const string& args) {
    print_report_line("Simulation Host", m_session->peer());
    print_report_line("VCML Version", m_session->vcml_version());
    print_report_line("SystemC Version", m_session->sysc_version());
    print_report_line("Simulation Time",
                      mwr::mkstr("%.9fs", m_session->time_ns() / 1e9));
    print_report_line("Delta Cycle", to_string(m_session->cycle()));
    print_report_line("CLI Version", VSP_VERSION_STRING);
    return true;
}

bool session::handle_run(const string& args) {
    if (m_session->running()) {
        cout << "already running" << endl;
        return true;
    }

    m_session->run();
    return true;
}

bool session::handle_stop(const string& args) {
    if (!m_session->running()) {
        cout << "not running" << endl;
        return true;
    }

    m_session->stop();
    while (m_session->running())
        ;
    cout << "stopped by " << m_session->reason() << endl;
    return true;
}

bool session::handle_quit(const string& args) {
    m_session->disconnect();
    return false;
}

bool session::handle_detach(const string& args) {
    return false;
}

bool session::handle_kill(const string& args) {
    try {
        m_session->kill();
    } catch (const mwr::report&) {
    }
    cout << "exiting" << endl;
    return false;
}

bool session::handle_exec(const string& args) {
    vector<string> split = mwr::split(args, ' ');

    vsp::command* c = m_current_mod->find_command(split[0]);
    if (!c) {
        cout << "command " << split[0] << " not found" << endl;
        return true;
    }

    vector<string> cmd_args(split.begin() + 1, split.end());
    string resp = c->execute(cmd_args);
    cout << resp << endl;
    return true;
}

string session::prompt() const {
    stringstream ss;
    ss << termcolors::BOLD << termcolors::WHITE << "[" << std::fixed
       << std::setprecision(9) << m_session->time_ns() / 1e9 << "s] "
       << termcolors::CLEAR;
    ss << termcolors::YELLOW << m_session->peer() << termcolors::CLEAR;
    ss << " " << termcolors::BOLD << termcolors::CYAN
       << m_current_mod->hierarchy_name() << termcolors::CLEAR;
    ss << endl;
    return ss.str();
}

bool session::before_run() {
    return m_session->is_connected();
}

} // namespace cli
