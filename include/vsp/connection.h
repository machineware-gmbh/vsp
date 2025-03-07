/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CONNECTION_H
#define VSP_CONNECTION_H

#include "vsp/cmn.h"

namespace vsp {

class connection
{
private:
    enum : char {
        ACK = '+',
        NACK = '-',
    };

    socket m_socket;
    mutex m_mtx;
    string m_host;
    u16 m_port;

    optional<string> recv();
    bool send(const string& data);

    static u8 checksum(const string& s);
    static string escape(const string& s);
    static vector<string> decompose(const string& s);

public:
    explicit connection(const string& host, u16 port);
    virtual ~connection() = default;

    connection() = delete;
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;

    bool is_connected() const;

    void connect();
    void disconnect();

    optional<vector<string>> command(const string& cmd);
    static bool check_response(const optional<vector<string>>& resp,
                               size_t p_cnt);

    const char* peer() const;
    const char* host() const;
    u16 port() const;
};

} // namespace vsp

#endif
