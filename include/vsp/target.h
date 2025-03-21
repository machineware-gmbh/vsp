/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_TARGET_H
#define VSP_TARGET_H

#include "vsp/common.h"
#include "vsp/connection.h"
#include "vsp/cpureg.h"

namespace vsp {

class target
{
private:
    connection& m_conn;
    string m_name;
    list<cpureg> m_regs;
    unordered_map<u64, u64> m_bp;

    bool update_regs();

public:
    explicit target(connection& conn, const string& name);
    virtual ~target() = default;

    target() = delete;
    target(const target&) = delete;
    target& operator=(const target&) = delete;

    const string& name() const;

    void step(size_t steps = 1);
    u64 virt_to_phys(u64 va);
    u64 insert_breakpoint(u64 addr);
    bool remove_breakpoint_id(u64 id);
    bool remove_breakpoint(u64 addr);

    vector<u8> read_vmem(u64 vaddr, size_t size);
    bool write_vmem(u64 vaddr, const vector<u8>& data);

    bool pc(u64& pc);
    list<cpureg>& regs();
    cpureg* find_reg(const string& name);
};

} // namespace vsp

#endif
