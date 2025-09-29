/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/connection.h"

namespace vsp {

connection::connection(const string& host, u16 port):
    m_socket(), m_mtx(), m_host(host), m_port(port) {
}

bool connection::is_connected() const {
    return m_socket.is_connected();
}

void connection::connect() {
    m_socket.connect(m_host, m_port);
}

void connection::disconnect() {
    m_socket.disconnect();
}

u8 connection::checksum(const string& s) {
    u8 result = 0;
    for (const char& c : s)
        result += static_cast<u8>(c);
    return result;
}

string connection::escape(const string& s) {
    string result;

    for (char ch : s) {
        if (ch == '$' || ch == '#' || ch == '*' || ch == '}')
            result += { '}', char(ch ^ 0x20) };
        else
            result += ch;
    }
    return result;
}

vector<string> connection::decompose(const string& s) {
    vector<string> l;
    string b;
    size_t i = 0;

    while (i < s.size()) {
        if (s[i] == '\\' && i < s.size() - 1) {
            b += s[i + 1];
            i += 2;
        } else if (s[i] == ',') {
            l.push_back(std::move(b));
            b = string();
            ++i;
        } else {
            b += s[i];
            i += 1;
        }
    }
    l.push_back(std::move(b));

    return l;
}

optional<string> connection::recv() {
    string packet;
    u8 checksum = 0;
    int repeat = 5; //  number of attempts to receive a valid response paket
    size_t maxlen = 10000000; // response length limit

    while (packet.size() < maxlen && m_socket.is_connected()) {
        char r = m_socket.recv_char();

        switch (r) {
        case '$':
            packet = "";
            checksum = 0;
            break;

        case '#': {
            u8 refsum = stoi(
                string({ static_cast<char>(m_socket.recv_char()),
                         static_cast<char>(m_socket.recv_char()) }),
                nullptr, 16);
            if (checksum == refsum) {
                m_socket.send_char(ACK);
                return packet;
            }

            m_socket.send_char(NACK);
            if (--repeat == 0)
                return nullopt;
            break;
        }

        case '}':
            checksum += static_cast<u8>(r);
            r = m_socket.recv_char();
            checksum += static_cast<u8>(r);
            packet += r ^ 0x20;
            break;

        default:
            checksum += static_cast<u8>(r);
            packet += r;
        }
    }

    return nullopt;
}

bool connection::send(const string& data) {
    if (!m_socket.is_connected())
        return false;

    string escaped_data = escape(data);
    stringstream ss;

    ss << '$' << escaped_data << '#' << std::hex << std::setw(2)
       << std::setfill('0') << static_cast<int>(checksum(escaped_data));

    try {
        for (int i = 0; i < 5; ++i) {
            m_socket.send(ss.str());
            if (m_socket.recv_char() == ACK)
                return true;
        }
    } catch (mwr::report&) {
    }

    disconnect();
    return false;
}

optional<vector<string>> connection::command(const string& cmd) {
    lock_guard lk(m_mtx);

    if (!send(cmd))
        return nullopt;

    optional<string> raw = recv();
    if (!raw)
        return nullopt;

    return decompose(raw.value());
}

bool connection::check_response(const optional<vector<string>>& resp,
                                size_t p_cnt) {
    if (!resp)
        return false;

    if (resp->size() != p_cnt)
        return false;

    if (resp->at(0) != "OK")
        return false;

    return true;
}

const char* connection::peer() const {
    return m_socket.peer();
}

const char* connection::host() const {
    return m_host.c_str();
}

u16 connection::port() const {
    return m_port;
}

} // namespace vsp
