/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_SESSION_H
#define VSP_SESSION_H

#include "vsp/common.h"
#include "vsp/attribute.h"
#include "vsp/command.h"
#include "vsp/connection.h"
#include "vsp/module.h"
#include "vsp/target.h"

namespace vsp {

class session
{
private:
    connection m_conn;
    string m_sysc_version;
    string m_vcml_version;
    bool m_running;
    string m_reason;
    unsigned long long m_time_ns;
    unsigned long long m_cycle;
    int m_quantum_ns;
    module* m_mods;
    list<target> m_targets;

    static list<shared_ptr<session>> local_sessions;

    void init();
    bool update_version();
    bool update_quantum();
    bool update_status();
    bool update_modules();

public:
    explicit session(const string& host, u16 port);
    virtual ~session() = default;

    session() = delete;
    session(const session&) = delete;
    session& operator=(const session&) = delete;

    bool running();
    const string& sysc_version() const;
    const string& vcml_version() const;
    unsigned long long time_ns();
    unsigned long long cycle();
    const string& reason() const;

    bool is_connected() const;
    void connect();
    void disconnect();
    void quit();
    void step(bool block = true);
    void step(u64 ns, bool block = true);
    void stepi(const target& t);
    void run();
    void stop();

    void dump();
    module* find_module(const string& name = "");
    attribute* find_attribute(const string& name);
    command* find_command(const string& name);
    target* find_target(const string& name);

    list<target>& targets();

    const char* peer() const;
    const char* host() const;
    u16 port() const;

    static list<shared_ptr<session>>& get_sessions();
};

} // namespace vsp

#endif
