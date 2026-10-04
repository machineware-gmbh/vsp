/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_DISPATCHER_H
#define VSP_DISPATCHER_H

#include <map>
#include <unordered_set>

#include "vsp/common.h"
#include "vsp/connection.h"
#include "vsp/events.h"

namespace vsp {

class module;

class dispatcher
{
public:
    using dropped_handler = function<void(u64)>;

    struct handlers {
        led_handler led;
        uart_handler uart;
        trace_handler trace_all;
        std::map<vsp_trace_protocol, trace_handler> trace;

        bool empty() const {
            return !led && !uart && !trace_all && trace.empty();
        }
    };

private:
    struct detached {
        string module;
        string event;
        handlers hs;
    };

    using key = pair<const module*, string>;

    connection& m_conn;
    module* m_root;
    const unordered_map<string, module*>* m_modules;

    mutable mutex m_mtx;
    std::map<key, handlers> m_handlers;

    std::map<string, std::map<const module*, size_t>> m_publishers;
    vector<detached> m_detached;
    dropped_handler m_on_dropped;
    u64 m_dropped;
    std::unordered_set<string> m_warned;

    module* find(const string& name) const;
    void command(const vector<string>& cmd);
    void send(const string& cmd, const string& event,
              const vector<const module*>& pubs);
    static void find_publishers(const module& mod, const string& event,
                                vector<const module*>& pubs);
    void subscribe(const module& mod, const string& event);
    void unsubscribe(const module& mod, const string& event);
    void update(const module& mod, const string& event,
                const function<void(handlers&)>& change);
    vector<handlers> collect(const module& sender, const string& name) const;

public:
    explicit dispatcher(connection& conn);
    virtual ~dispatcher() = default;

    dispatcher() = delete;
    dispatcher(const dispatcher&) = delete;
    dispatcher& operator=(const dispatcher&) = delete;

    void on_led(const module& mod, led_handler fn);
    void on_uart(const module& mod, uart_handler fn);
    void on_trace(const module& mod, trace_handler fn);
    void on_trace(const module& mod, vsp_trace_protocol protocol,
                  trace_handler fn);
    bool is_subscribed(const module& mod, const string& event) const;

    void on_dropped(dropped_handler fn);
    u64 dropped() const;

    // passes the events field of a status response to the handlers
    void dispatch(const string& events);

    // called by the session after loading and before deleting the modules
    void attach(module* root, const unordered_map<string, module*>* mods);
    void detach();
};

} // namespace vsp

#endif
