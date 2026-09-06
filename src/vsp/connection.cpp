/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsp/connection.h"

namespace vsp {

static const int MAX_RETRIES = 5;

static u8 checksum(const string& s) {
    u8 result = 0;
    for (const char& c : s)
        result += static_cast<u8>(c);
    return result;
}

static string rsp_escape(const string& s) {
    string result;

    for (char ch : s) {
        if (ch == '$' || ch == '#' || ch == '*' || ch == '}')
            result += { '}', char(ch ^ 0x20) };
        else
            result += ch;
    }
    return result;
}

static string vsp_escape(const string& s) {
    string result;
    for (char ch : s) {
        if (ch == '\\' || ch == ',')
            result += '\\';
        result += ch;
    }
    return result;
}

static vector<string> decompose(const string& s) {
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

connection::connection(): m_mtx(), m_socket() {
    // nothing to do
}

connection::connection(const string& host, u16 port): connection() {
    connect(host, port);
}

connection::connection(connection&& other) noexcept:
    m_mtx(), m_socket(std::move(other.m_socket)) {
}

void connection::connect(const string& host, u16 port) {
    m_socket.connect(host, port);
}

void connection::disconnect() noexcept {
    m_socket.disconnect();
}

string connection::recv() {
    string packet;
    u8 checksum = 0;
    int repeat = MAX_RETRIES;

    while (m_socket.is_connected()) {
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
                MWR_REPORT("server nack while receiving");
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

    MWR_REPORT("server response too long");
}

void connection::send(const string& data) {
    if (!m_socket.is_connected())
        MWR_REPORT("not connected");

    string escaped_data = rsp_escape(data);
    stringstream ss;

    ss << '$' << escaped_data << '#' << std::hex << std::setw(2)
       << std::setfill('0') << static_cast<int>(checksum(escaped_data));

    try {
        for (int i = 0; i < MAX_RETRIES; i++) {
            m_socket.send(ss.str());
            if (m_socket.recv_char() == ACK)
                return;
        }

        MWR_REPORT("server nack while sending");
    } catch (mwr::report&) {
        disconnect();
        throw;
    }
}

vector<string> connection::command(const vector<string>& cmd) {
    lock_guard lk(m_mtx);

    string escaped_cmd;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (i > 0)
            escaped_cmd += ',';
        escaped_cmd += vsp_escape(cmd[i]);
    }

    send(escaped_cmd);
    auto resp = decompose(recv());
    if (resp.empty())
        MWR_REPORT("server sent empty response");
    if (resp.at(0) != "OK") {
        string errmsg = "unknown error";
        if (resp.size() > 1)
            errmsg = resp.at(1);
        MWR_REPORT("%s", errmsg.c_str());
    }

    return resp;
}

} // namespace vsp
