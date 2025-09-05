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

enum stop_reason_t {
    VSP_STOP_REASON_USER,
    VSP_STOP_REASON_BREAKPOINT,
    VSP_STOP_REASON_STEP_COMPLETE,
    VSP_STOP_REASON_RWATCHPOINT,
    VSP_STOP_REASON_WWATCHPOINT,
    VSP_STOP_REASON_COUNT
};

namespace vsp {

struct stop_reason {
    static constexpr size_t DATA_SIZE = 16;
    stop_reason_t reason = VSP_STOP_REASON_COUNT;
    union {
        struct {
            u64 id;
            u64 time;
        } breakpoint;
        struct {
            target* tgt;
            u64 time;
        } step_complete;
        struct {
            u64 id;
            u64 addr;
            u64 size;
            u64 time;
        } rwatchpoint;
        struct {
            u64 id;
            u64 addr;
            u8 data[DATA_SIZE];
            u64 time;
        } wwatchpoint;
    };

    string str() const {
        switch (reason) {
        case VSP_STOP_REASON_USER:
            return "user";
        case VSP_STOP_REASON_STEP_COMPLETE:
            return "target";
            break;
        case VSP_STOP_REASON_BREAKPOINT:
            return "breakpoint";
        case VSP_STOP_REASON_RWATCHPOINT:
            return "rwatchpoint";
        case VSP_STOP_REASON_WWATCHPOINT:
            return "wwatchpoint";
        default:
            return "<unknown>";
        }
        return "<unknown>";
    }
};

string reason_str(const stop_reason& type);

class session
{
private:
    connection m_conn;
    string m_sysc_version;
    string m_vcml_version;
    bool m_running;
    stop_reason m_reason;
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
    void update_reason(const string& reason);

public:
    explicit session(const string& host, u16 port);
    virtual ~session();

    session() = delete;
    session(const session&) = delete;
    session& operator=(const session&) = delete;

    bool running();
    const string& sysc_version() const;
    const string& vcml_version() const;
    unsigned long long time_ns();
    unsigned long long cycle();
    const stop_reason& reason() const;

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
