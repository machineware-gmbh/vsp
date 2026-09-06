/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CONNECTION_H
#define VSP_CONNECTION_H

#include "vsp/common.h"

namespace vsp {

class connection
{
private:
    enum : char {
        ACK = '+',
        NACK = '-',
    };

    mutex m_mtx;
    socket m_socket;

    string recv();
    void send(const string& data);

public:
    connection();
    connection(const string& host, u16 port);
    connection(connection&& other) noexcept;
    virtual ~connection() = default;

    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;

    const char* peer() const { return m_socket.peer(); }
    const char* host() const { return m_socket.host(); }
    u16 port() const { return m_socket.port(); }

    bool is_connected() const { return m_socket.is_connected(); }

    void connect(const string& host, u16 port);
    void disconnect() noexcept;

    vector<string> command(const vector<string>& cmd);
};

} // namespace vsp

#endif
