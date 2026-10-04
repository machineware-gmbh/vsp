/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_MODULE_H
#define VSP_MODULE_H

#include "vsp/common.h"
#include "vsp/element.h"
#include "vsp/events.h"

namespace vsp {

class module : public element
{
private:
    dispatcher& m_dispatcher;
    string m_kind;
    string m_version;
    vector<string> m_events;
    vector<module*> m_mods;
    vector<attribute*> m_attrs;
    vector<command*> m_cmds;

    friend class dispatcher;
    bool publishes(const string& event) const;
    void on_trace(vsp_trace_protocol protocol, trace_handler fn);

public:
    module(const string& name, connection& conn, dispatcher& disp,
           module* parent, const string& kind, const string& version,
           const vector<string>& events = {});
    virtual ~module();
    module() = delete;
    module(const module&) = delete;
    module& operator=(const module&) = delete;

    const char* kind() const;
    const char* version() const;

    // events published by this module itself, e.g. "trace" for sockets
    const vector<string>& events() const { return m_events; }

    bool is_traceable() const { return publishes("trace"); }
    bool has_leds() const { return publishes("led"); }
    bool has_uart() const { return publishes("uart"); }

    // whether a trace handler is installed on this module
    bool is_traced() const;

    void on_led(led_handler fn);
    void on_uart(uart_handler fn);
    void on_trace(trace_handler fn);

    template <typename T>
    void on_trace(function<void(const trace_info&, const T&)> fn);

    friend ostream& operator<<(ostream& os, const module& mod);

    module* find_module(const string& mod);
    attribute* find_attribute(const string& name);
    command* find_command(const string& name);

    void add_module(module* mod);
    void add_attribute(attribute* attr);
    void add_attribute(const string& name, const string& type, size_t count);
    void add_command(command* c);
    void add_command(const string& name, size_t argc, const string& desc);

    const vector<module*>& children() const { return m_mods; }
    const vector<attribute*>& attributes() const { return m_attrs; }
    const vector<command*>& commands() const { return m_cmds; }
};

template <typename T>
void module::on_trace(function<void(const trace_info&, const T&)> fn) {
    if (!fn) {
        on_trace(T::PROTOCOL, nullptr);
        return;
    }

    on_trace(T::PROTOCOL,
             [fn](const trace_info& info, const trace_payload& tx) {
                 if (const T* payload = std::get_if<T>(&tx))
                     fn(info, *payload);
             });
}

} // namespace vsp

#endif
