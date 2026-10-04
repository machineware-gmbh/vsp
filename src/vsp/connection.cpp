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
    size_t pos = 0;

    while (true) {
        // copy everything up to the next separator or escape at once
        size_t next = s.find_first_of("\\,", pos);
        b.append(s, pos, next == string::npos ? string::npos : next - pos);
        if (next == string::npos)
            break;

        if (s[next] == ',') {
            l.push_back(std::move(b));
            b.clear();
            pos = next + 1;
        } else if (next + 1 < s.size()) {
            b += s[next + 1]; // escaped character
            pos = next + 2;
        } else {
            b += s[next]; // trailing backslash
            pos = next + 1;
        }
    }

    l.push_back(std::move(b));
    return l;
}

connection::connection(): m_mtx(), m_socket(), m_rxbuf(), m_rxpos(0) {
    // nothing to do
}

connection::connection(const string& host, u16 port): connection() {
    connect(host, port);
}

connection::connection(connection&& other) noexcept:
    m_mtx(),
    m_socket(std::move(other.m_socket)),
    m_rxbuf(std::move(other.m_rxbuf)),
    m_rxpos(other.m_rxpos) {
}

void connection::connect(const string& host, u16 port) {
    m_rxbuf.clear();
    m_rxpos = 0;
    m_socket.connect(host, port);
}

void connection::disconnect() noexcept {
    m_socket.disconnect();
    m_rxbuf.clear();
    m_rxpos = 0;
}

void connection::fill() {
    // block for one byte, then take everything that is already available;
    // reading byte by byte is too slow for large responses
    m_rxbuf.clear();
    m_rxpos = 0;
    m_rxbuf.push_back((char)m_socket.recv_char());

    try {
        for (size_t n = m_socket.peek(0); n > 0; n = m_socket.peek(0)) {
            size_t pos = m_rxbuf.size();
            m_rxbuf.resize(pos + n);
            m_socket.recv(m_rxbuf.data() + pos, n);
        }
    } catch (std::exception&) {
        // disconnected, the next call reports it
    }
}

char connection::recv_char() {
    if (m_rxpos == m_rxbuf.size())
        fill();
    return m_rxbuf[m_rxpos++];
}

string connection::recv() {
    string packet;
    u8 checksum = 0;
    int repeat = MAX_RETRIES;

    while (m_socket.is_connected()) {
        if (m_rxpos == m_rxbuf.size())
            fill();

        // take all plain characters at once, only $, # and } need handling
        const char* data = m_rxbuf.data();
        size_t end = m_rxpos;
        while (end < m_rxbuf.size() && data[end] != '$' && data[end] != '#' &&
               data[end] != '}') {
            checksum += static_cast<u8>(data[end++]);
        }

        packet.append(data + m_rxpos, end - m_rxpos);
        m_rxpos = end;
        if (m_rxpos == m_rxbuf.size())
            continue;

        char r = recv_char();
        switch (r) {
        case '$':
            packet = "";
            checksum = 0;
            break;

        case '#': {
            u8 refsum = stoi(string({ static_cast<char>(recv_char()),
                                      static_cast<char>(recv_char()) }),
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

        default: // '}'
            checksum += static_cast<u8>(r);
            r = recv_char();
            checksum += static_cast<u8>(r);
            packet += r ^ 0x20;
            break;
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
            if (recv_char() == ACK)
                return;
        }

        MWR_REPORT("server nack while sending");
    } catch (mwr::report&) {
        disconnect();
        throw;
    }
}

vector<string> connection::request(const vector<string>& cmd) {
    lock_guard lk(m_mtx);

    string escaped_cmd;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (i > 0)
            escaped_cmd += ',';
        escaped_cmd += vsp_escape(cmd[i]);
    }

    send(escaped_cmd);
    return decompose(recv());
}

vector<string> connection::command(const vector<string>& cmd) {
    auto resp = request(cmd);
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
