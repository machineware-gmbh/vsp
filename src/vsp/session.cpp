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

namespace vsp {

list<shared_ptr<session>> session::local_sessions;

session::session(const string& host, u16 port):
    m_conn(host, port),
    m_sysc_version(),
    m_vcml_version(),
    m_running(false),
    m_reason(),
    m_time_ns(0),
    m_cycle(0),
    m_quantum_ns(0),
    m_mods(nullptr),
    m_targets() {
}

session::~session() {
    disconnect();
}

bool session::update_version() {
    optional<vector<string>> resp = m_conn.command("version");

    if (!connection::check_response(resp, 3))
        return false;

    m_sysc_version = resp->at(1);
    m_vcml_version = resp->at(2);

    return true;
}

bool session::update_quantum() {
    optional<vector<string>> resp = m_conn.command("getq");

    if (!connection::check_response(resp, 2))
        return false;

    m_quantum_ns = stoi(resp->at(1));

    return true;
}

bool session::update_status() {
    optional<vector<string>> resp = m_conn.command("status");

    if (!connection::check_response(resp, 4))
        return false;

    if (resp->at(1) == "running") {
        m_running = true;
    } else if (resp->at(1).compare("stopped:") >= 0) {
        m_running = false;
        m_reason = resp->at(1).substr(8);
    }

    m_time_ns = stoull(resp->at(2));
    m_cycle = stoull(resp->at(3));

    return true;
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

unsigned long long session::time_ns() {
    update_status();
    return m_time_ns;
}

unsigned long long session::cycle() {
    update_status();
    return m_cycle;
}

const string& session::reason() const {
    return m_reason;
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
    update_quantum();
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
    step(m_quantum_ns, block);
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

        string host;
        getline(file, host);

        string data;
        getline(file, data);
        u16 port = stoi(data);

        size_t count = 0;
        while (getline(file, data))
            count++;

        if (count != 2)
            continue;

        for (const auto& s : local_sessions) {
            if (s->m_conn.host() == host && s->m_conn.port() == port)
                continue;
        }

        local_sessions.emplace_back(make_shared<session>(host, port));
    }
    return local_sessions;
}

} // namespace vsp
