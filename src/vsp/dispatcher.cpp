/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsp/dispatcher.h"
#include "vsp/connection.h"
#include "vsp/module.h"

#include "vsp/payloads.h"

namespace vsp {

dispatcher::dispatcher(connection& conn):
    m_conn(conn),
    m_root(nullptr),
    m_modules(nullptr),
    m_mtx(),
    m_handlers(),
    m_publishers(),
    m_detached(),
    m_on_dropped(),
    m_dropped(0),
    m_warned() {
}

module* dispatcher::find(const string& name) const {
    if (name.empty())
        return m_root;
    if (m_modules == nullptr)
        return nullptr;
    auto it = m_modules->find(name);
    return it != m_modules->end() ? it->second : nullptr;
}

void dispatcher::command(const vector<string>& cmd) {
    // older simulators send an empty response to unknown commands
    auto resp = m_conn.request(cmd);
    if (resp.empty() || (resp.size() == 1 && resp[0].empty()))
        MWR_REPORT("events not supported by simulator");

    if (resp[0] != "OK")
        MWR_REPORT("%s", resp.size() > 1 ? resp[1].c_str() : "unknown error");
}

// all objects below mod (including mod) that publish event themselves
void dispatcher::find_publishers(const module& mod, const string& event,
                                 vector<const module*>& pubs) {
    if (mwr::stl_contains(mod.m_events, event))
        pubs.push_back(&mod);
    for (const module* child : mod.children())
        find_publishers(*child, event, pubs);
}

void dispatcher::send(const string& cmd, const string& event,
                      const vector<const module*>& pubs) {
    vector<string> args{ cmd, event };
    args.reserve(2 + pubs.size());
    for (const module* pub : pubs)
        args.push_back(pub->hierarchy_name());
    command(args);
}

void dispatcher::subscribe(const module& mod, const string& event) {
    vector<const module*> pubs;
    find_publishers(mod, event, pubs);
    if (pubs.empty()) {
        MWR_REPORT("'%s' does not publish '%s' events",
                   mod.hierarchy_name().c_str(), event.c_str());
    }

    // only subscribe publishers that no other handler needs yet
    vector<const module*> added;
    {
        lock_guard<mutex> guard(m_mtx);
        auto& counts = m_publishers[event];
        for (const module* pub : pubs) {
            if (counts[pub] == 0)
                added.push_back(pub);
        }
    }

    if (!added.empty())
        send("sub", event, added);

    lock_guard<mutex> guard(m_mtx);
    auto& counts = m_publishers[event];
    for (const module* pub : pubs)
        counts[pub]++;
}

void dispatcher::unsubscribe(const module& mod, const string& event) {
    vector<const module*> pubs;
    find_publishers(mod, event, pubs);

    // only unsubscribe publishers that no other handler needs anymore
    vector<const module*> removed;
    {
        lock_guard<mutex> guard(m_mtx);
        auto& counts = m_publishers[event];
        for (const module* pub : pubs) {
            if (counts[pub] == 1)
                removed.push_back(pub);
        }
    }

    if (!removed.empty())
        send("unsub", event, removed);

    lock_guard<mutex> guard(m_mtx);
    auto& counts = m_publishers[event];
    for (const module* pub : pubs) {
        if (counts[pub] > 0 && --counts[pub] == 0)
            counts.erase(pub);
    }
}

void dispatcher::update(const module& mod, const string& event,
                        const function<void(handlers&)>& change) {
    key k(&mod, event);
    handlers hs;
    bool subscribed;

    {
        lock_guard<mutex> guard(m_mtx);
        auto it = m_handlers.find(k);
        subscribed = it != m_handlers.end();
        if (subscribed)
            hs = it->second;
    }

    change(hs);

    // the first handler subscribes and the last one unsubscribes
    if (!subscribed && !hs.empty())
        subscribe(mod, event);
    else if (subscribed && hs.empty())
        unsubscribe(mod, event);

    lock_guard<mutex> guard(m_mtx);
    if (hs.empty())
        m_handlers.erase(k);
    else
        m_handlers[k] = std::move(hs);
}

void dispatcher::on_led(const module& mod, led_handler fn) {
    update(mod, "led", [&](handlers& hs) { hs.led = std::move(fn); });
}

void dispatcher::on_uart(const module& mod, uart_handler fn) {
    update(mod, "uart", [&](handlers& hs) { hs.uart = std::move(fn); });
}

void dispatcher::on_trace(const module& mod, trace_handler fn) {
    update(mod, "trace", [&](handlers& hs) { hs.trace_all = std::move(fn); });
}

void dispatcher::on_trace(const module& mod, vsp_trace_protocol protocol,
                          trace_handler fn) {
    update(mod, "trace", [&](handlers& hs) {
        if (fn)
            hs.trace[protocol] = std::move(fn);
        else
            hs.trace.erase(protocol);
    });
}

bool dispatcher::is_subscribed(const module& mod, const string& event) const {
    lock_guard<mutex> guard(m_mtx);
    return m_handlers.count(key(&mod, event)) > 0;
}

void dispatcher::on_dropped(dropped_handler fn) {
    lock_guard<mutex> guard(m_mtx);
    m_on_dropped = std::move(fn);
}

u64 dispatcher::dropped() const {
    lock_guard<mutex> guard(m_mtx);
    return m_dropped;
}

vector<dispatcher::handlers> dispatcher::collect(const module& sender,
                                                 const string& name) const {
    // handlers of the sender and all its parents, called without the lock
    // held so that they can add or remove handlers themselves
    vector<handlers> found;
    lock_guard<mutex> guard(m_mtx);
    for (const module* mod = &sender; mod; mod = mod->parent()) {
        auto it = m_handlers.find(key(mod, name));
        if (it != m_handlers.end())
            found.push_back(it->second);
    }

    return found;
}

static void notify_led(const vector<dispatcher::handlers>& found,
                       module& sender, u64 time_ps, u64 delta,
                       const json& payload) {
    auto index = payload.find("led");
    auto state = payload.find("state");
    if (index == payload.end() || !index->is_number_unsigned() ||
        state == payload.end() || !state->is_boolean()) {
        return;
    }

    led_event ev{ sender, time_ps, delta, index->get<size_t>(),
                  state->get<bool>() };
    for (const auto& hs : found) {
        if (hs.led)
            hs.led(ev);
    }
}

static void notify_trace(const vector<dispatcher::handlers>& found,
                         module& sender, u64 time_ps, u64 delta,
                         const json& payload) {
    auto dir = payload.find("dir");
    auto prot = payload.find("protocol");
    auto err = payload.find("error");
    auto tx = payload.find("tx");
    if (dir == payload.end() || !dir->is_string() || prot == payload.end() ||
        !prot->is_string() || tx == payload.end()) {
        return;
    }

    const string& name = prot->get_ref<const string&>();
    vsp_trace_protocol protocol = trace_protocol_from_str(name.c_str());

    // only parse the payload if a handler wants it
    bool wanted = false;
    for (const auto& hs : found)
        wanted |= hs.trace_all || hs.trace.count(protocol);
    if (!wanted)
        return;

    optional<trace_payload> data = parse_payload(protocol, name, *tx);
    if (!data)
        return;

    vsp_trace_dir d = *dir == "bw" ? VSP_TRACE_BW : VSP_TRACE_FW;
    bool e = err != payload.end() && err->is_boolean() && err->get<bool>();
    trace_info info{ sender, time_ps, delta, d, e };

    for (const auto& hs : found) {
        if (hs.trace_all)
            hs.trace_all(info, *data);
        auto it = hs.trace.find(protocol);
        if (it != hs.trace.end())
            it->second(info, *data);
    }
}

void dispatcher::dispatch(const string& events) {
    json obj = json::parse(events, nullptr, false);
    if (obj.is_discarded() || !obj.is_object()) {
        log_warn("ignoring invalid events: %s", events.c_str());
        return;
    }

    auto dropped = obj.find("dropped");
    if (dropped != obj.end() && dropped->is_number_unsigned()) {
        u64 n = dropped->get<u64>();
        dropped_handler handler;
        {
            lock_guard<mutex> guard(m_mtx);
            m_dropped += n;
            handler = m_on_dropped;
        }

        if (handler)
            handler(n);
    }

    auto list = obj.find("events");
    if (list == obj.end() || !list->is_array())
        return;

    // consecutive uart characters of the same sender are delivered together
    optional<uart_event> uart;
    auto flush_uart = [&]() {
        if (!uart)
            return;
        for (const auto& hs : collect(uart->terminal, "uart")) {
            if (hs.uart)
                hs.uart(*uart);
        }
        uart.reset();
    };

    for (const auto& ev : *list) {
        auto name = ev.find("event");
        auto sender = ev.find("sender");
        auto time = ev.find("time");
        auto delta = ev.find("delta");
        auto payload = ev.find("payload");
        if (name == ev.end() || !name->is_string() || sender == ev.end() ||
            !sender->is_string() || time == ev.end() ||
            !time->is_number_unsigned() || delta == ev.end() ||
            !delta->is_number_unsigned() || payload == ev.end()) {
            continue;
        }

        const string& sname = sender->get_ref<const string&>();
        module* mod = find(sname);
        if (mod == nullptr) {
            if (m_warned.insert(sname).second)
                log_warn("ignoring events of unknown object '%s'",
                         sname.c_str());
            continue;
        }

        const string& evname = name->get_ref<const string&>();
        u64 t = time->get<u64>();
        u64 d = delta->get<u64>();

        if (evname == "uart") {
            optional<char> c = parse_uart(*payload);
            if (!c)
                continue;
            if (uart && &uart->terminal != mod)
                flush_uart();
            if (!uart)
                uart.emplace(uart_event{ *mod, t, d, string() });
            uart->data += *c;
            continue;
        }

        flush_uart();

        auto found = collect(*mod, evname);
        if (found.empty())
            continue;

        if (evname == "led")
            notify_led(found, *mod, t, d, *payload);
        else if (evname == "trace")
            notify_trace(found, *mod, t, d, *payload);
    }

    flush_uart();
}

void dispatcher::attach(module* root,
                        const unordered_map<string, module*>* mods) {
    vector<detached> previous;
    {
        lock_guard<mutex> guard(m_mtx);
        m_root = root;
        m_modules = mods;
        previous.swap(m_detached);
    }

    // subscribe again with the handlers of the previous connection
    for (detached& d : previous) {
        module* mod = find(d.module);
        if (mod == nullptr) {
            log_warn("dropping '%s' handlers of '%s'", d.event.c_str(),
                     d.module.c_str());
            continue;
        }

        try {
            update(*mod, d.event, [&](handlers& hs) { hs = std::move(d.hs); });
        } catch (std::exception& ex) {
            log_warn("failed to subscribe to '%s' events of '%s': %s",
                     d.event.c_str(), d.module.c_str(), ex.what());
        }
    }
}

void dispatcher::detach() {
    lock_guard<mutex> guard(m_mtx);
    if (m_root == nullptr)
        return;

    for (auto& [k, hs] : m_handlers)
        m_detached.push_back({ k.first->hierarchy_name(), k.second, hs });

    m_handlers.clear();
    m_publishers.clear();
    m_root = nullptr;
    m_modules = nullptr;
}

} // namespace vsp
