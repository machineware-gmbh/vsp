/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_TESTING_H
#define VSP_TESTING_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "vsp.h"

template <typename T>
bool try_connect(T& session, const std::string& host, mwr::u16 port) {
    try {
        session.connect(host, port);
    } catch (...) {
        return false;
    }
    return true;
}

template <typename T>
bool try_connect(T& session, const std::string& host, mwr::u16 port,
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

#ifdef SIMPLE_VP_PATH

inline mwr::u16 launch_simple_vp(mwr::subprocess& subp,
                                 const std::vector<std::string>& extra = {},
                                 int timeout_ms = 10000) {
    std::vector<std::string> args{ "--session" };
    args.insert(args.end(), extra.begin(), extra.end());
    MWR_REPORT_ON(!subp.run(SIMPLE_VP_PATH, args),
                  "failed to launch simple_vp");

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        for (const auto& info : vsp::session::local_sessions()) {
            if (info.pid == (mwr::u32)subp.pid() && info.port != 0)
                return info.port;
        }
        mwr::usleep(1000);
    }

    MWR_REPORT("simple_vp did not announce its session");
}

template <typename T>
mwr::u16 connect_simple_vp(T& session, mwr::subprocess& subp,
                           const std::vector<std::string>& extra = {},
                           int timeout_ms = 10000) {
    mwr::u16 port = launch_simple_vp(subp, extra, timeout_ms);
    MWR_REPORT_ON(!try_connect(session, "localhost", port, timeout_ms),
                  "failed to connect to simple_vp on port %hu", port);
    return port;
}

#endif // SIMPLE_VP_PATH

#endif
