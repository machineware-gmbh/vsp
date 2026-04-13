/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_TESTING_H
#define VSP_TESTING_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "vsp.h"

template <typename T>
static bool try_connect(T& session, const std::string& host, mwr::u16 port) {
    try {
        session.connect(host, port);
    } catch (...) {
        return false;
    }
    return true;
}

template <typename T>
static bool try_connect(T& session, const std::string& host, mwr::u16 port,
                        int timeout) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout);

    while (std::chrono::steady_clock::now() < deadline) {
        if (try_connect(session, host, port))
            return true;
        mwr::usleep(100);
    }

    return false;
}

#endif
