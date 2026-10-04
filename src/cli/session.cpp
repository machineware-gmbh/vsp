/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "cli/session.h"

#include <atomic>
#include <csignal>

#include "vsp/version.h"

namespace cli {

session::session(shared_ptr<vsp::session> s):
    m_session(std::move(s)),
    m_current_mod(nullptr),
    m_last_cmd(),
    m_event_lines(),
    m_event_skipped(0),
    m_event_file(),
    m_event_stream(false),
    m_subscriptions() {
    m_current_mod = m_session->find_module("");

    register_handler(&session::handle_cd, "cd",
                     "moves current module to <module>");
    register_handler(&session::handle_exec, "exec",
                     "executes the given <command> [args...]", "x");
    register_handler(&session::handle_info, "info",
                     "print information about the current session", "i");
    register_handler(
        &session::handle_list, "list",
        "displays the module hierarchy onwards from current module", "ls");
    register_handler(&session::handle_quit, "quit", "terminate session", "q");
    register_handler(&session::handle_detach, "detach",
                     "disconnect from session", "d");
    register_handler(&session::handle_read, "read",
                     "reads the given <attribute>", "r");
    register_handler(&session::handle_run, "run",
                     "continues simulation, use CTRL+C to interrupt", "c");
    register_handler(&session::handle_stop, "stop", "stops the simulation");
    register_handler(&session::handle_step, "step",
                     "advances simulation to the next discrete timestamp",
                     "s");
    register_handler(&session::handle_events, "events",
                     "simulation events: add|rm <event> <obj>..., "
                     "log [file]; no arguments shows the status");
    register_handler(&session::handle_trace, "trace",
                     "traces all ports of the given modules, or of the "
                     "current module");
    register_handler(&session::handle_untrace, "untrace",
                     "stops tracing the given modules, or the current module");

    m_session->on_events_dropped([this](mwr::u64 n) {
        stringstream ss;
        ss << "--- " << n << " events dropped ---";
        m_event_lines.push_back(ss.str());
    });
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
        for (auto& m : m_current_mod->children()) {
            cout << termcolors::BOLD << termcolors::CYAN << m->name()
                 << termcolors::CLEAR;
            for (const auto& [event, mod] : m_subscriptions) {
                if (mod == m)
                    cout << " [" << event << "]";
            }
            cout << endl;
        }
    }

    if (show_attr) {
        for (auto& a : m_current_mod->attributes()) {
            cout << termcolors::WHITE << a->name() << termcolors::CLEAR
                 << endl;
        }
    }

    if (show_cmd) {
        vsp::command* cinfo = m_current_mod->find_command("cinfo");
        for (auto& c : m_current_mod->commands()) {
            cout << termcolors::BOLD << termcolors::MAGENTA << c->name()
                 << termcolors::CLEAR;

            if (cinfo)
                cout << " " << cinfo->execute({ c->name() });
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
    m_session->step(-1);
    return true;
}

bool session::handle_info(const string& args) {
    print_report_line("Simulation Host", m_session->peer());
    print_report_line("VCML Version", m_session->vcml_version());
    print_report_line("SystemC Version", m_session->sysc_version());
    print_report_line("Proto. Version", m_session->proto_version());
    print_report_line("Simulation Time",
                      mwr::mkstr("%.9fs", m_session->get_time_ns() / 1e9));
    print_report_line("Delta Cycle", to_string(m_session->get_cycle_count()));
    print_report_line("CLI Version", VSP_VERSION_STRING);
    return true;
}

static std::atomic<bool> interrupted(false);

static void handle_sigint(int sig) {
    interrupted = true;
}

bool session::handle_run(const string& args) {
    if (m_session->check_running()) {
        cout << "already running" << endl;
        return true;
    }

    interrupted = false;
    auto prev = std::signal(SIGINT, handle_sigint);

    m_session->run();
    m_event_stream = true;
    while (!interrupted && m_session->check_running()) {
        cout << flush_events() << std::flush;
        mwr::usleep(10000);
    }

    if (interrupted) {
        m_session->stop();
        while (m_session->check_running())
            ;
    }

    m_event_stream = false;
    std::signal(SIGINT, prev);
    cout << flush_events() << "stopped by " << m_session->reason() << endl;
    return true;
}

bool session::handle_stop(const string& args) {
    if (!m_session->check_running()) {
        cout << "not running" << endl;
        return true;
    }

    m_session->stop();
    while (m_session->check_running())
        ;
    cout << "stopped by " << m_session->reason() << endl;
    return true;
}

bool session::handle_detach(const string& args) {
    m_session->disconnect();
    return false;
}

bool session::handle_quit(const string& args) {
    try {
        m_session->quit();
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

bool session::handle_events(const string& args) {
    vector<string> split = mwr::split(args, ' ');
    split.erase(std::remove(split.begin(), split.end(), ""), split.end());

    try {
        if (split.empty())
            return events_status();

        const string& sub = split[0];
        vector<string> rest(split.begin() + 1, split.end());

        if (sub == "add")
            return events_select(rest, true);
        if (sub == "rm")
            return events_select(rest, false);
        if (sub == "log")
            return events_log(rest);

        cout << "unknown events command '" << sub << "'" << endl;
    } catch (std::exception& ex) {
        cout << "events: " << ex.what() << endl;
    }

    return true;
}

bool session::handle_trace(const string& args) {
    return trace_select(args, true);
}

bool session::handle_untrace(const string& args) {
    return trace_select(args, false);
}

bool session::trace_select(const string& args, bool enable) {
    vector<string> split = mwr::split(args, ' ');
    split.erase(std::remove(split.begin(), split.end(), ""), split.end());
    if (split.empty())
        split.push_back("");
    std::replace(split.begin(), split.end(), string("."), string());
    split.insert(split.begin(), "trace");

    try {
        return events_select(split, enable);
    } catch (std::exception& ex) {
        cout << (enable ? "trace: " : "untrace: ") << ex.what() << endl;
    }

    return true;
}

bool session::events_status() {
    print_report_line("Output", m_event_file.is_open() ? "file" : "console");
    print_report_line("Dropped", m_session->events_dropped());
    for (const auto& [event, mod] : m_subscriptions)
        print_report_line(event, mod->hierarchy_name());
    return true;
}

bool session::events_select(const vector<string>& args, bool enable) {
    if (args.size() < 2) {
        cout << "usage: events " << (enable ? "add" : "rm")
             << " trace|led|uart <obj>..." << endl;
        return true;
    }

    const string& event = args[0];
    if (event != "trace" && event != "led" && event != "uart") {
        cout << "unknown event '" << event << "', use trace, led or uart"
             << endl;
        return true;
    }

    for (size_t i = 1; i < args.size(); i++) {
        vsp::module* mod = m_current_mod->find_module(args[i]);
        if (!mod) {
            cout << "module '" << args[i] << "' does not exist!" << endl;
            return true;
        }

        if (event == "trace") {
            vsp::trace_handler fn = nullptr;
            if (enable) {
                fn = [this](const vsp::trace_info& info,
                            const vsp::trace_payload& tx) {
                    stringstream ss;
                    ss << info << " " << tx;
                    print_event(ss.str());
                };
            }
            mod->on_trace(fn);
        } else if (event == "led") {
            vsp::led_handler fn = nullptr;
            if (enable) {
                fn = [this](const vsp::led_event& ev) {
                    print_event(mwr::mkstr(
                        "%s %s led %zu=%d",
                        vsp::format_time(ev.time_ps).c_str(),
                        ev.leds.hierarchy_name().c_str(), ev.index, ev.state));
                };
            }
            mod->on_led(fn);
        } else {
            vsp::uart_handler fn = nullptr;
            if (enable) {
                fn = [this](const vsp::uart_event& ev) {
                    stringstream ss;
                    ss << vsp::format_time(ev.time_ps) << " "
                       << ev.terminal.hierarchy_name() << " uart \"";
                    for (char c : ev.data) {
                        if (c == '\n')
                            ss << "\\n";
                        else if (c >= 0x20 && c < 0x7f)
                            ss << c;
                        else
                            ss << mwr::mkstr("\\x%02x", (mwr::u8)c);
                    }
                    ss << "\"";
                    print_event(ss.str());
                };
            }
            mod->on_uart(fn);
        }

        auto entry = std::make_pair(event, mod);
        if (enable)
            mwr::stl_add_unique(m_subscriptions, entry);
        else
            mwr::stl_remove(m_subscriptions, entry);
    }

    return true;
}

bool session::events_log(const vector<string>& args) {
    if (m_event_file.is_open())
        m_event_file.close();

    if (!args.empty()) {
        m_event_file.open(args[0]);
        if (!m_event_file)
            cout << "cannot open '" << args[0] << "'" << endl;
    }

    return true;
}

void session::print_event(const string& line) {
    if (m_event_file.is_open()) {
        m_event_file << line << '\n';
        return;
    }

    if (m_event_stream) {
        cout << line << '\n';
        return;
    }

    if (m_event_lines.size() >= MAX_EVENT_LINES) {
        m_event_skipped++;
        return;
    }

    m_event_lines.push_back(line);
}

string session::flush_events() const {
    stringstream ss;
    for (const string& line : m_event_lines)
        ss << line << endl;
    if (m_event_skipped > 0)
        ss << "... " << m_event_skipped << " more events not shown" << endl;

    m_event_lines.clear();
    m_event_skipped = 0;
    return ss.str();
}

string session::prompt() const {
    // fetching the time also delivers pending events
    double time = m_session->get_time_ns() / 1e9;

    stringstream ss;
    ss << flush_events();
    ss << termcolors::BOLD << termcolors::WHITE << "[" << std::fixed
       << std::setprecision(9) << time << "s] " << termcolors::CLEAR;
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
