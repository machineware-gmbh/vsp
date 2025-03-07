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
    connection conn("localhost", 1234);
    EXPECT_STREQ(conn.host(), "localhost");
    EXPECT_EQ(conn.port(), 1234);
    EXPECT_STREQ(conn.peer(), "");
    EXPECT_FALSE(conn.is_connected());
}

bool try_connect(connection& conn) {
    try {
        conn.connect();
    } catch (...) {
        return false;
    }
    return true;
}

TEST(connection, connect) {
    mwr::socket server;
    connection conn("localhost", 1234);

    EXPECT_FALSE(server.is_connected());
    EXPECT_FALSE(conn.is_connected());
    EXPECT_FALSE(try_connect(conn));
    EXPECT_FALSE(server.is_connected());
    EXPECT_FALSE(conn.is_connected());

    server.listen(1234);
    EXPECT_TRUE(server.is_listening());

    EXPECT_FALSE(server.is_connected());
    EXPECT_FALSE(conn.is_connected());
    EXPECT_TRUE(try_connect(conn));
    EXPECT_FALSE(server.is_connected());
    EXPECT_TRUE(conn.is_connected());

    server.accept();
    EXPECT_TRUE(server.is_connected());

    EXPECT_THAT(conn.peer(), testing::AnyOf(testing::StrEq("127.0.0.1:1234"),
                                            testing::StrEq("::1:1234")));
}

TEST(connection, disconnect) {
    mwr::socket server;
    connection conn("localhost", 1234);

    server.listen(1234);

    EXPECT_TRUE(try_connect(conn));
    EXPECT_TRUE(conn.is_connected());
    server.accept();
    EXPECT_TRUE(server.is_connected());

    conn.disconnect();
    EXPECT_FALSE(conn.is_connected());
    EXPECT_TRUE(server.is_connected());

    try {
        server.recv_char();
        FAIL();
    } catch (...) {
    }

    EXPECT_FALSE(server.is_connected());
}

TEST(connection, server_disconnect) {
    mwr::socket server;
    connection conn("localhost", 1234);

    server.listen(1234);

    EXPECT_TRUE(try_connect(conn));
    EXPECT_TRUE(conn.is_connected());
    server.accept();
    EXPECT_TRUE(server.is_connected());

    server.disconnect();
    EXPECT_FALSE(server.is_connected());
    EXPECT_TRUE(conn.is_connected());

    auto resp = conn.command("test");
    EXPECT_FALSE(resp.has_value());
}

TEST(connection, send) {
    mwr::socket server;
    connection conn("localhost", 1234);

    server.listen(1234);

    EXPECT_TRUE(try_connect(conn));
    EXPECT_TRUE(conn.is_connected());
    server.accept();
    EXPECT_TRUE(server.is_connected());

    std::future<bool> correct = std::async([&server]() {
        std::string_view expected_msg = "$test#c0";
        for (const char& c : expected_msg) {
            if (server.recv_char() != c)
                return false;
        }

        server.send("+");

        std::string_view reply = "$OK,myarg#e6";
        server.send(reply.data(), reply.size());

        return server.recv_char() == '+';
    });
    auto resp = conn.command("test");
    correct.wait();
    EXPECT_TRUE(correct.get());

    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(), 2);
    EXPECT_EQ(resp->at(0), "OK");
    EXPECT_EQ(resp->at(1), "myarg");
}

TEST(connection, nack) {
    mwr::socket server;
    connection conn("localhost", 1234);

    server.listen(1234);

    EXPECT_TRUE(try_connect(conn));
    EXPECT_TRUE(conn.is_connected());
    server.accept();
    EXPECT_TRUE(server.is_connected());

    std::future<bool> correct = std::async([&server]() {
        std::string_view expected_msg = "$test#c0";
        for (const char& c : expected_msg) {
            if (server.recv_char() != c)
                return false;
        }

        server.send("-");

        for (const char& c : expected_msg) {
            if (server.recv_char() != c)
                return false;
        }

        server.send("+");

        std::string_view reply_wrong = "$OK,myarg#e4";
        server.send(reply_wrong.data(), reply_wrong.size());

        if (server.recv_char() != '-')
            return false;

        std::string_view reply = "$OK,myarg#e6";
        server.send(reply.data(), reply.size());

        return server.recv_char() == '+';
    });
    auto resp = conn.command("test");
    correct.wait();
    EXPECT_TRUE(correct.get());

    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(), 2);
    EXPECT_EQ(resp->at(0), "OK");
    EXPECT_EQ(resp->at(1), "myarg");
}
