/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_ELEMENT_H
#define VSP_ELEMENT_H

#include "vsp/cmn.h"
#include "vsp/connection.h"

namespace vsp {

class module;

class element
{
protected:
    connection& m_conn;
    module* m_parent;
    string m_name;

public:
    explicit element(const string& name, connection& conn,
                     module* parent = nullptr);
    virtual ~element() = default;

    element() = delete;
    element(const element&) = delete;
    element& operator=(const element&) = delete;

    const char* name() const;
    string hierarchy_name() const;
};

} // namespace vsp

#endif
