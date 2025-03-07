/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CPUREG_H
#define VSP_CPUREG_H

#include "vsp/cmn.h"
#include "vsp/connection.h"

namespace vsp {

class target;

class cpureg
{
private:
    connection& m_conn;
    string m_name;
    size_t m_size;
    target& m_parent;

    bool update_size();

public:
    explicit cpureg(connection& conn, const string& name, target& m_parent);
    virtual ~cpureg() = default;

    cpureg() = delete;
    cpureg(const cpureg&) = delete;
    cpureg& operator=(const cpureg&) = delete;

    size_t size() const;
    bool get_value(vector<u8>& ret);
    bool set_value(const vector<u8>& ret);
    const char* name() const;
};

} // namespace vsp

#endif
