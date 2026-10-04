/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsp/module.h"

#include "vsp/attribute.h"
#include "vsp/command.h"
#include "vsp/dispatcher.h"

namespace vsp {

module::module(const string& name, connection& conn, dispatcher& disp,
               module* parent, const string& kind, const string& version,
               const vector<string>& events) :element(name, conn, parent),
    m_dispatcher(disp), m_kind(kind), m_version(version), m_events(events) {
}

module::~module() {
    for (auto* mod : m_mods)
        delete mod;
    for (auto* attr : m_attrs)
        delete attr;
    for (auto* cmd : m_cmds)
        delete cmd;
}

const char* module::kind() const {
    return m_kind.c_str();
}

const char* module::version() const {
    return m_version.c_str();
}

bool module::publishes(const string& event) const {
    if (mwr::stl_contains(m_events, event))
        return true;

    for (const module* child : m_mods) {
        if (child->publishes(event))
            return true;
    }

    return false;
}

bool module::is_traced() const {
    return m_dispatcher.is_subscribed(*this, "trace");
}

void module::on_led(led_handler fn) {
    m_dispatcher.on_led(*this, std::move(fn));
}

void module::on_uart(uart_handler fn) {
    m_dispatcher.on_uart(*this, std::move(fn));
}

void module::on_trace(trace_handler fn) {
    m_dispatcher.on_trace(*this, std::move(fn));
}

void module::on_trace(vsp_trace_protocol protocol, trace_handler fn) {
    m_dispatcher.on_trace(*this, protocol, std::move(fn));
}

module* module::find_module(const string& mod) {
    if (mod == "")
        return this;

    size_t dot_pos = mod.find('.');
    string mod_name = dot_pos == string::npos ? mod : mod.substr(0, dot_pos);
    auto it = find_if(m_mods.begin(), m_mods.end(),
                      [&mod_name](const class module* m) -> bool {
                          return strcmp(m->name(), mod_name.c_str()) == 0;
                      });

    if (it == m_mods.end())
        return nullptr;

    if (dot_pos == string::npos)
        return *it;

    return (*it)->find_module(mod.substr(dot_pos + 1));
}

attribute* module::find_attribute(const string& name) {
    size_t dot_pos = name.find_last_of('.');

    if (dot_pos == string::npos) {
        auto it = find_if(m_attrs.begin(), m_attrs.end(),
                          [&name](const class attribute* a) -> bool {
                              return strcmp(a->name(), name.c_str()) == 0;
                          });
        if (it == m_attrs.end())
            return nullptr;
        return *it;
    }

    module* module = find_module(name.substr(0, dot_pos));
    if (!module)
        return nullptr;

    return module->find_attribute(name.substr(dot_pos + 1));
}

command* module::find_command(const string& name) {
    size_t dot_pos = name.find_last_of('.');

    if (dot_pos == string::npos) {
        auto it = find_if(m_cmds.begin(), m_cmds.end(),
                          [&name](const command* c) -> bool {
                              return strcmp(c->name(), name.c_str()) == 0;
                          });
        if (it == m_cmds.end())
            return nullptr;
        return *it;
    }

    module* module = find_module(name.substr(0, dot_pos));
    if (!module)
        return nullptr;

    return module->find_command(name.substr(dot_pos + 1));
}

void module::add_module(module* mod) {
    m_mods.push_back(mod);
}

void module::add_attribute(attribute* attr) {
    m_attrs.push_back(attr);
}

void module::add_attribute(const string& name, const string& type,
                           size_t count) {
    add_attribute(new attribute(name, m_conn, this, type, count));
}

void module::add_command(command* c) {
    m_cmds.push_back(c);
}

void module::add_command(const string& name, size_t argc, const string& desc) {
    add_command(new command(name, m_conn, this, argc, desc));
}

ostream& operator<<(ostream& os, const module& mod) {
    os << mod.hierarchy_name() << " (" << mod.kind() << ")" << endl;

    for (const auto* attr : mod.m_attrs)
        os << "  " << attr->name() << ": " << attr->type() << endl;

    for (const auto* mod : mod.m_mods)
        os << *mod;

    return os;
}

} // namespace vsp
