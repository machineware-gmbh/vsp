/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/session.h"

#include "vsp/connection.h"
#include "vsp/module.h"

#include <pugixml.hpp>

using std::stoi;
using std::stoull;

namespace vsp {

list<shared_ptr<session>> session::local_sessions;

// converts a string of bytes to a vector of bytes
// "ddccbbaa" -> { aa, bb, cc, dd }
static void strhex(u8* buffer, size_t buflen, const string& bytes) {
    if (!buffer)
        return;

    memset(buffer, 0, buflen);

    if (bytes.empty())
        return;

    if (bytes.size() & 1) {
        log_error("corrupted byte string of size %lu", bytes.size());
        return;
    }

    size_t src_idx = std::min(bytes.size(), buflen * 2);
    size_t dst_idx = 0;
    constexpr size_t chars_per_byte = 2;

    while (src_idx != 0) {
        src_idx -= chars_per_byte;
        string chunk = bytes.substr(src_idx, chars_per_byte);
        buffer[dst_idx++] = (u8)stoi(chunk, 0, 16);
    }
}

static const unordered_map<std::string_view, stop_reason_t> VSP_STOP_REASONS{
    { "user", VSP_STOP_REASON_USER },
    { "breakpoint", VSP_STOP_REASON_BREAKPOINT },
    { "target", VSP_STOP_REASON_TARGET_STEP_COMPLETE },
    { "step", VSP_STOP_REASON_STEP_COMPLETE },
    { "rwatchpoint", VSP_STOP_REASON_RWATCHPOINT },
    { "wwatchpoint", VSP_STOP_REASON_WWATCHPOINT },
    { "<unknown>", VSP_STOP_REASON_UNKNOWN },
};

static stop_reason_t stop_reason_from_string(const string& s) {
    if (!VSP_STOP_REASONS.count(s))
        return VSP_STOP_REASON_UNKNOWN;

    return VSP_STOP_REASONS.at(s);
}

string_view stop_reason_str(const stop_reason& reason) {
    for (auto& r : VSP_STOP_REASONS) {
        if (r.second == reason.reason)
            return r.first;
    }

    return "<unknown>";
}

ostream& operator<<(ostream& out, const stop_reason& reason) {
    return out << stop_reason_str(reason);
}

session::session(const string& host, u16 port):
    m_conn(host, port),
    m_sysc_version(),
    m_vcml_version(),
    m_protover(0),
    m_running(false),
    m_reason(),
    m_time_ns(0),
    m_cycle(0),
    m_mods(nullptr),
    m_targets() {
}

session::~session() {
    disconnect();
}

bool session::update_version() {
    optional<vector<string>> resp = m_conn.command("version");

    if (!(connection::check_response(resp) &&
          (resp->size() == 3 || resp->size() == 4)))
        return false;

    m_sysc_version = resp->at(1);
    m_vcml_version = resp->at(2);

    if (resp->size() > 3)
        m_protover = stoi(resp->at(3));

    return true;
}

bool session::update_status() {
    optional<vector<string>> resp = m_conn.command("status");

    if (!is_connected()) {
        m_running = false;
        return false;
    }

    if (!connection::check_response(resp, 4))
        return false;

    if (resp->at(1) == "running") {
        m_running = true;
    } else {
        m_running = false;
        update_reason(resp->at(1).substr(8));
    }

    m_time_ns = stoull(resp->at(2));
    m_cycle = stoull(resp->at(3));

    return true;
}

void session::update_reason(const string& reason) {
    if (reason.empty())
        return;

    auto args = split(reason, ':');

    stop_reason newreason;
    if (args.size() < 1)
        newreason.reason = VSP_STOP_REASON_UNKNOWN;
    else
        newreason.reason = stop_reason_from_string(args[0]);

    switch (newreason.reason) {
    case VSP_STOP_REASON_STEP_COMPLETE:
    case VSP_STOP_REASON_USER: {
        // nothing to do
        break;
    }

    case VSP_STOP_REASON_BREAKPOINT: {
        if (args.size() != 3) {
            newreason.reason = VSP_STOP_REASON_UNKNOWN;
            break;
        }

        newreason.breakpoint.id = stoull(args[1], 0, 10);
        newreason.breakpoint.time = stoull(args[2], 0, 10);
        break;
    }

    case VSP_STOP_REASON_TARGET_STEP_COMPLETE: {
        if (args.size() != 3) {
            newreason.reason = VSP_STOP_REASON_UNKNOWN;
            break;
        }

        newreason.target_step_complete.tgt = find_target(args[1]);
        newreason.target_step_complete.time = stoull(args[2], 0, 10);
        break;
    }

    case VSP_STOP_REASON_RWATCHPOINT: {
        if (args.size() != 5) {
            newreason.reason = VSP_STOP_REASON_UNKNOWN;
            break;
        }

        newreason.rwatchpoint.id = stoull(args[1], 0, 10);
        newreason.rwatchpoint.addr = stoull(args[2], 0, 16);
        newreason.rwatchpoint.size = stoull(args[3], 0, 10);
        newreason.rwatchpoint.time = stoull(args[4], 0, 10);
        break;
    }

    case VSP_STOP_REASON_WWATCHPOINT: {
        if (args.size() != 5) {
            newreason.reason = VSP_STOP_REASON_UNKNOWN;
            break;
        }

        newreason.wwatchpoint.id = stoull(args[1], 0, 10);
        newreason.wwatchpoint.addr = stoull(args[2], 0, 16);

        const string& data = args[3];
        if (data.size() > stop_reason::DATA_SIZE) {
            log_error("wwatchpoint: written %lu bytes (>%lu), data dropped",
                      data.size(), stop_reason::DATA_SIZE);
        }

        strhex(newreason.wwatchpoint.data, stop_reason::DATA_SIZE, data);

        newreason.wwatchpoint.time = stoull(args[4], 0, 10);
        break;
    }

    case VSP_STOP_REASON_UNKNOWN:
    default:
        newreason.reason = VSP_STOP_REASON_UNKNOWN;
        break;
    }

    m_reason = newreason;
}

module* xml_parse_modules(connection& conn, const pugi::xml_node& node,
                          module* parent) {
    module* mod = new module(node.attribute("name").value(), conn, parent,
                             node.attribute("kind").value(),
                             node.attribute("version").value());

    for (auto& child : node.children("object")) {
        mod->add_module(xml_parse_modules(conn, child, mod));
    }

    for (auto& target : node.children("attribute")) {
        mod->add_attribute(target.attribute("name").value(),
                           target.attribute("type").value(),
                           target.attribute("count").as_ullong());
    }

    for (auto& cmd : node.children("command")) {
        mod->add_command(cmd.attribute("name").value(),
                         cmd.attribute("argc").as_ullong(),
                         cmd.attribute("desc").value());
    }

    return mod;
}

bool session::update_modules() {
    optional<vector<string>> resp = m_conn.command("list,xml");

    if (!connection::check_response(resp, 2))
        return false;

    pugi::xml_document list;
    if (!list.load_string(resp->at(1).c_str()))
        return false;

    pugi::xml_node hierachy = list.child("hierarchy");

    if (m_mods != nullptr)
        delete m_mods;
    m_mods = xml_parse_modules(m_conn, hierachy, nullptr);

    for (auto& t : hierachy.children("target"))
        m_targets.emplace_back(m_conn, t.text().as_string());

    return true;
}

bool session::running() {
    update_status();
    return m_running;
}

const string& session::sysc_version() const {
    return m_sysc_version;
}

const string& session::vcml_version() const {
    return m_vcml_version;
}

int session::proto_version() const {
    return m_protover;
}

unsigned long long session::time_ns() {
    update_status();
    return m_time_ns;
}

unsigned long long session::cycle() {
    update_status();
    return m_cycle;
}

unsigned long long session::quantum_ns() {
    optional<vector<string>> resp = m_conn.command("getq");

    if (!connection::check_response(resp, 2))
        return 0;

    return stoi(resp->at(1));
}

void session::set_quantum(unsigned long long ns) {
    auto resp = m_conn.command("setq," + to_string(ns));
    MWR_REPORT_ON(!connection::check_response(resp, 1),
                  "set quantum failed (%s)", response_get_error(resp).c_str());
}

void session::connect() {
    if (m_conn.is_connected())
        return;

    try {
        m_conn.connect();
    } catch (mwr::report&) {
        return;
    }

    if (!is_connected())
        return;

    update_version();
    update_status();

    stop();
    while (running())
        mwr::cpu_yield();

    update_modules();
}

void session::disconnect() {
    m_conn.disconnect();

    if (m_mods != nullptr)
        delete m_mods;
    m_mods = nullptr;
}

bool session::is_connected() const {
    return m_conn.is_connected();
}

void session::quit() {
    try {
        m_conn.command("quit");
    } catch (mwr::report&) {
        // excpect disconnect
    }
    disconnect();
}

void session::step(bool block) {
    step(quantum_ns(), block);
}

void session::step(u64 ns, bool block) {
    update_status();
    if (!m_running) {
        m_running = true;
        m_conn.command("resume," + to_string(ns) + "ns");
    }

    if (block) {
        while (m_running)
            update_status();
    }
}

void session::stepi(const target& t) {
    update_status();
    if (!m_running) {
        m_running = true;
        m_conn.command("step," + string(t.name()));
    }

    while (m_running)
        update_status();
}

void session::run() {
    update_status();
    if (!m_running) {
        m_running = true;
        m_conn.command("resume");
    }
}

void session::stop() {
    update_status();
    if (m_running)
        m_conn.command("stop");
}

void session::dump() {
    if (!m_mods)
        return;

    cout << *m_mods;
}

module* session::find_module(const string& name) {
    if (!m_mods)
        return nullptr;

    return m_mods->find_module(name);
}

attribute* session::find_attribute(const string& name) {
    if (!m_mods)
        return nullptr;

    return m_mods->find_attribute(name);
}

command* session::find_command(const string& name) {
    if (!m_mods)
        return nullptr;

    return m_mods->find_command(name);
}

target* session::find_target(const string& name) {
    for (auto& t : m_targets) {
        if (strcmp(t.name(), name.c_str()) == 0)
            return &t;
    }

    return nullptr;
}

list<target>& session::targets() {
    return m_targets;
}

const char* session::peer() const {
    return m_conn.peer();
}

const char* session::host() const {
    return m_conn.host();
}

u16 session::port() const {
    return m_conn.port();
}

list<shared_ptr<session>>& session::get_sessions() {
    for (const auto& f : fs::directory_iterator(mwr::temp_dir())) {
        if (f.path().filename().string().find("vcml_session_") != 0)
            continue;
        ifstream file(f.path());
        if (!file.is_open())
            continue;

        string host, data;
        if (!getline(file, host) || !getline(file, data))
            continue;

        u16 port = stoi(data);

        for (const auto& s : local_sessions) {
            if (s->m_conn.host() == host && s->m_conn.port() == port)
                continue;
        }

        local_sessions.emplace_back(make_shared<session>(host, port));
    }
    return local_sessions;
}

} // namespace vsp
