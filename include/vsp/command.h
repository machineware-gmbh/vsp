/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_COMMAND_H
#define VSP_COMMAND_H

#include "vsp/common.h"
#include "vsp/element.h"

namespace vsp {

class command : public element
{
private:
    size_t m_argc;
    string m_desc;

public:
    explicit command(const string& name, connection& conn, module* parent,
                     size_t argc, const string& desc);
    virtual ~command() = default;

    command() = delete;
    command(const command&) = delete;
    command& operator=(const command&) = delete;

    string execute(const vector<string>& args);
    string execute(const string& args);

    const char* desc() const;
    size_t argc() const;
};

} // namespace vsp

#endif
