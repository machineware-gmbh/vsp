/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef CLI_STARTUP_H
#define CLI_STARTUP_H

#include "cli/cli.h"

#include <vsp.h>

namespace cli {

class startup : public cli
{
private:
    shared_ptr<vsp::session> m_session;
    vector<shared_ptr<vsp::session>> m_sessions;

    bool handle_exit(const string& args);
    bool handle_list(const string& args);
    bool handle_select(const string& args);
    bool handle_connect(const string& args);

    void session();

public:
    startup();
    virtual ~startup() = default;

    startup(const startup&) = delete;
    startup& operator=(const startup&) = delete;
};

} // namespace cli

#endif
