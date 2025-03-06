/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CLI_SESSION_CLI_H
#define VSP_CLI_SESSION_CLI_H

#include "vsp-cli/cli.h"

#include <vsp.h>

namespace vsp {

class session_cli : public cli
{
private:
    shared_ptr<session> m_session;
    module* m_current_mod;
    string m_last_cmd;

    bool handle_list(const string& args);
    bool handle_info(const string& args);
    bool handle_cd(const string& args);
    bool handle_read(const string& args);
    bool handle_step(const string& args);
    bool handle_run(const string& args);
    bool handle_stop(const string& args);
    bool handle_quit(const string& args);
    bool handle_detach(const string& args);
    bool handle_kill(const string& args);
    bool handle_exec(const string& args);

    void print_report_line(const string& kind, const string& value);
    bool print();

protected:
    string prompt() const override;
    bool before_run() override;

public:
    explicit session_cli(shared_ptr<session> s);
    virtual ~session_cli() = default;

    session_cli() = delete;
    session_cli(const session_cli&) = delete;
    session_cli& operator=(const session_cli&) = delete;
};

} // namespace vsp

#endif
