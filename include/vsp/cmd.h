/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CMD_H
#define VSP_CMD_H

#include "vsp/cmn.h"
#include "vsp/element.h"

namespace vsp {

class cmd : public element
{
private:
    size_t m_argc;
    string m_desc;

public:
    explicit cmd(const string& name, connection& conn, module* parent,
                 size_t argc, const string& desc);
    virtual ~cmd() = default;

    cmd() = delete;
    cmd(const cmd&) = delete;
    cmd& operator=(const cmd&) = delete;

    string execute(const vector<string>& args);
    string execute(const string& args);

    const char* desc() const;
    size_t argc() const;
};

} // namespace vsp

#endif
