/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

#include <future>
#include <string_view>

using namespace vsp;

TEST(connection, constructor) {
    connection conn;
    EXPECT_STREQ(conn.host(), "");
    EXPECT_EQ(conn.port(), 0);
    EXPECT_STREQ(conn.peer(), "");
    EXPECT_FALSE(conn.is_connected());
}

TEST(connection, connect) {
    connection conn;

    EXPECT_FALSE(conn.is_connected());
    EXPECT_FALSE(try_connect(conn, "localhost", 12345));
    EXPECT_FALSE(conn.is_connected());

    mwr::server_socket server(1, 0);
    EXPECT_TRUE(server.is_listening());

    EXPECT_FALSE(server.is_connected());
    EXPECT_FALSE(conn.is_connected());
    EXPECT_TRUE(try_connect(conn, server.host(), server.port()));
    EXPECT_FALSE(server.is_connected());
    EXPECT_TRUE(conn.is_connected());

    server.poll(100);
    EXPECT_TRUE(server.is_connected());

    string peer = mkstr("%s:%hu", server.host(), server.port());
    EXPECT_EQ(conn.peer(), peer);
}

TEST(connection, disconnect) {
    mwr::server_socket server(1, 0);
    connection conn;

    EXPECT_TRUE(try_connect(conn, server.host(), server.port()));
    EXPECT_TRUE(conn.is_connected());

    server.poll(100);
    EXPECT_TRUE(server.is_connected());

    conn.disconnect();
    EXPECT_FALSE(conn.is_connected());
    EXPECT_TRUE(server.is_connected());

    try {
        server.recv_char(0);
        FAIL();
    } catch (...) {
    }

    EXPECT_FALSE(server.is_connected());
}

TEST(connection, server_disconnect) {
    mwr::server_socket server(1, 0);
    connection conn;

    EXPECT_TRUE(try_connect(conn, server.host(), server.port()));
    EXPECT_TRUE(conn.is_connected());

    server.poll(100);
    EXPECT_TRUE(server.is_connected());

    server.disconnect_all();
    EXPECT_FALSE(server.is_connected());
    EXPECT_TRUE(conn.is_connected());

    EXPECT_THROW(conn.command("test"), std::exception);
}

TEST(connection, send) {
    mwr::server_socket server(1, 0);
    connection conn;

    EXPECT_TRUE(try_connect(conn, server.host(), server.port()));
    EXPECT_TRUE(conn.is_connected());

    server.poll(100);
    EXPECT_TRUE(server.is_connected());
    int client = server.clients()[0];

    std::future<bool> correct = std::async([&server, client]() {
        std::string_view expected_msg = "$test#c0";
        for (const char& c : expected_msg) {
            if (server.recv_char(client) != c)
                return false;
        }

        server.send(client, "+");

        std::string_view reply = "$OK,myarg#e6";
        server.send(client, reply.data(), reply.size());

        return server.recv_char(client) == '+';
    });

    auto resp = conn.command("test");
    correct.wait();
    EXPECT_TRUE(correct.get());
    EXPECT_EQ(resp.size(), 2);
    EXPECT_EQ(resp[0], "OK");
    EXPECT_EQ(resp[1], "myarg");
}

TEST(connection, nack) {
    mwr::server_socket server(1, 0);
    connection conn;

    EXPECT_TRUE(try_connect(conn, server.host(), server.port()));
    EXPECT_TRUE(conn.is_connected());

    server.poll(100);
    EXPECT_TRUE(server.is_connected());
    int client = server.clients()[0];

    std::future<bool> correct = std::async([&server, client]() {
        std::string_view expected_msg = "$test#c0";
        for (const char& c : expected_msg) {
            if (server.recv_char(client) != c)
                return false;
        }

        server.send(client, "-");

        for (const char& c : expected_msg) {
            if (server.recv_char(client) != c)
                return false;
        }

        server.send(client, "+");

        std::string_view reply_wrong = "$OK,myarg#e4";
        server.send(client, reply_wrong.data(), reply_wrong.size());

        if (server.recv_char(client) != '-')
            return false;

        std::string_view reply = "$OK,myarg#e6";
        server.send(client, reply.data(), reply.size());

        return server.recv_char(client) == '+';
    });
    auto resp = conn.command("test");
    correct.wait();
    EXPECT_TRUE(correct.get());

    EXPECT_EQ(resp.size(), 2);
    EXPECT_EQ(resp[0], "OK");
    EXPECT_EQ(resp[1], "myarg");
}
