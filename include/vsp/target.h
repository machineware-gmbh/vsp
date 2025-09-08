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

struct breakpoint {
    u64 addr;
    u64 id;
};

enum watchpoint_type {
    WP_READ = 1,
    WP_WRITE = 2,
    WP_ACCESS = WP_READ | WP_WRITE
};

struct watchpoint {
    u64 addr;
    u64 size;
    u64 id;
    watchpoint_type type;
};

class target
{
private:
    connection& m_conn;
    string m_name;
    list<cpureg> m_regs;

    bool update_regs();

public:
    explicit target(connection& conn, const string& name);
    virtual ~target() = default;

    target() = delete;
    target(const target&) = delete;
    target& operator=(const target&) = delete;

    const char* name() const;

    void step(size_t steps = 1);
    u64 virt_to_phys(u64 va);

    optional<breakpoint> insert_breakpoint(u64 addr);
    bool remove_breakpoint(const breakpoint& bp);

    optional<watchpoint> insert_watchpoint(u64 addr, u64 size,
                                           watchpoint_type type);
    bool remove_watchpoint(const watchpoint& wp);

    vector<u8> read_vmem(u64 vaddr, size_t size);
    bool write_vmem(u64 vaddr, const vector<u8>& data);

    bool pc(u64& pc);
    list<cpureg>& regs();
    cpureg* find_reg(const string& name);
};

} // namespace vsp

#endif
