/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/module.h"

#include "vsp/attribute.h"
#include "vsp/cmd.h"

namespace vsp {

module::module(const string& name, connection& conn, module* parent,
               const string& kind,
               const string& version) :element(name, conn, parent),
    m_kind(kind), m_version(version) {
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
                              return a->name() == name;
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

cmd* module::find_command(const string& name) {
    size_t dot_pos = name.find_last_of('.');

    if (dot_pos == string::npos) {
        auto it = find_if(
            m_cmds.begin(), m_cmds.end(),
            [&name](const class cmd* c) -> bool { return c->name() == name; });
        if (it == m_cmds.end())
            return nullptr;
        return *it;
    }

    module* module = find_module(name.substr(0, dot_pos));
    if (!module)
        return nullptr;

    return module->find_command(name.substr(dot_pos + 1));
}

module* module::parent() {
    return m_parent;
}

const list<module*>& module::get_modules() {
    return m_mods;
}

const list<attribute*>& module::get_attritbutes() {
    return m_attrs;
}

const list<cmd*>& module::get_cmds() {
    return m_cmds;
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

void module::add_cmd(cmd* c) {
    m_cmds.push_back(c);
}

void module::add_cmd(const string& name, size_t argc, const string& desc) {
    add_cmd(new cmd(name, m_conn, this, argc, desc));
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
