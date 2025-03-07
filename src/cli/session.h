/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef CLI_SESSION_H
#define CLI_SESSION_H

#include "cli/cli.h"

#include "cli/cmn.h"
#include <vsp.h>

namespace cli {

class session : public cli
{
private:
    shared_ptr<vsp::session> m_session;
    vsp::module* m_current_mod;
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
    explicit session(shared_ptr<vsp::session> s);
    virtual ~session() = default;

    session() = delete;
    session(const session&) = delete;
    session& operator=(const session&) = delete;
};

} // namespace cli

#endif
