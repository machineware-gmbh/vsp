/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CLI_STARTUP_CLI_H
#define VSP_CLI_STARTUP_CLI_H

#include "vsp-cli/cli.h"

#include <vsp.h>

namespace vsp {

class startup_cli : public cli
{
private:
    shared_ptr<session> m_session;
    vector<shared_ptr<session>> m_sessions;

    bool handle_exit(const string& args);
    bool handle_list(const string& args);
    bool handle_select(const string& args);
    bool handle_connect(const string& args);

    void session_cli();

public:
    startup_cli();
    virtual ~startup_cli() = default;

    startup_cli(const startup_cli&) = delete;
    startup_cli& operator=(const startup_cli&) = delete;
};

} // namespace vsp

#endif
