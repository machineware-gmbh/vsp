/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_MODULE_H
#define VSP_MODULE_H

#include "vsp/cmn.h"
#include "vsp/element.h"

namespace vsp {

class attribute;
class cmd;

class module : public element
{
private:
    string m_kind;
    string m_version;
    list<module*> m_mods;
    list<attribute*> m_attrs;
    list<cmd*> m_cmds;

public:
    explicit module(const string& name, connection& conn, module* parent,
                    const string& kind, const string& version);
    virtual ~module();
    module() = delete;
    module(const module&) = delete;
    module& operator=(const module&) = delete;

    const char* kind() const;
    const char* version() const;

    friend ostream& operator<<(ostream& os, const module& mod);

    module* find_module(const string& mod);
    attribute* find_attribute(const string& name);
    cmd* find_command(const string& name);

    module* parent();

    void add_module(module* mod);
    void add_attribute(attribute* attr);
    void add_attribute(const string& name, const string& type, size_t count);
    void add_cmd(cmd* c);
    void add_cmd(const string& name, size_t argc, const string& desc);

    const list<module*>& get_modules();
    const list<attribute*>& get_attritbutes();
    const list<cmd*>& get_cmds();
};

} // namespace vsp

#endif
