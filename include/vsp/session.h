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

enum vsp_proto_version {
    VSP_UNKNOWN = 0,
    VSP_V1 = 1,
};

enum vsp_stop_mode {
    VSP_STOP_MODE_SOFT = 0,
    VSP_STOP_MODE_HARD,
};

enum vsp_stop_reason {
    VSP_STOP_REASON_UNKNOWN = 0,
    VSP_STOP_REASON_USER,
    VSP_STOP_REASON_BREAKPOINT,
    VSP_STOP_REASON_TARGET_STEP_COMPLETE,
    VSP_STOP_REASON_STEP_COMPLETE,
    VSP_STOP_REASON_RWATCHPOINT,
    VSP_STOP_REASON_WWATCHPOINT,
    VSP_STOP_REASON_COUNT,
};

struct stop_reason {
    static constexpr size_t DATA_SIZE = 16;
    vsp_stop_reason reason;
    union {
        struct {
            u64 id;
            u64 time;
        } breakpoint;
        struct {
            target* tgt;
            u64 time;
        } target_step_complete;
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
};

string_view stop_reason_str(const stop_reason& reason);
ostream& operator<<(ostream& out, const stop_reason& reason);

struct session_info {
    string host;
    u16 port;
};

class session
{
private:
    connection m_conn;
    string m_sysc_version;
    string m_vcml_version;
    int m_protover;
    bool m_running;
    stop_reason m_reason;
    u64 m_time_ns;
    u64 m_cycle;
    module* m_mods;
    vector<target*> m_targets;

    void update_version();
    void update_status();
    void update_modules();
    void update_reason(const string& reason);

public:
    session();
    session(const session_info& info);
    session(const string& host, u16 port);
    session(session&& other) noexcept;
    virtual ~session();

    session(const session&) = delete;
    session& operator=(const session&) = delete;

    const char* sysc_version() const;
    const char* vcml_version() const;
    int proto_version() const;

    u64 get_time_ns();
    u64 get_cycle_count();

    u64 get_quantum_ns();
    void set_quantum(u64 ns);

    const char* peer() const { return m_conn.peer(); }
    const char* host() const { return m_conn.host(); }
    u16 port() const { return m_conn.port(); }

    bool is_connected() const;
    void connect(const session_info& info);
    void connect(const string& host, u16 port);
    void disconnect() noexcept;

    void step(bool block = true);
    void step(u64 ns, bool block = true);
    void stepi(const target& t);

    void run();
    bool check_running();
    void stop();
    void set_stop_mode(vsp_stop_mode mode);
    const stop_reason& reason() const { return m_reason; }

    void quit();

    void dump(ostream& os = std::cout);

    module* find_module(const string& name = "");
    attribute* find_attribute(const string& name);
    command* find_command(const string& name);
    target* find_target(const string& name);

    const vector<target*>& targets() const { return m_targets; }
    const vector<module*>& modules() const;

    static vector<session_info> local_sessions();
};

} // namespace vsp

#endif
