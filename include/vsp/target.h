/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_TARGET_H
#define VSP_TARGET_H

#include "vsp/common.h"
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
    u64 base;
    u64 size;
    u64 id;
    watchpoint_type type;
};

enum vmem_status {
    VMEM_OK = 0,
    VMEM_MMU_FAULT = 1,
    VMEM_BUS_ERROR = 2,
};

struct symbol {
    string name;
    bool is_code;
    u64 addr;
    u64 size;
};

struct instruction {
    u64 virt;
    optional<u64> phys;
    vector<u8> bytes;
    string symbol;
    u64 offset;
    string code;
};

struct stackframe {
    u64 pc;
    u64 fp;
    string symbol;
    u64 offset;
};

struct target_info {
    u64 id;
    u64 pc;
    u64 lr;
    u64 sp;
    u64 fp;
};

class target;

struct target_group {
    string name;
    vector<target*> targets;
    target* find_target(const string& name) const;
};

class target
{
private:
    connection& m_conn;
    string m_name;
    string m_arch;
    target_group& m_group;
    vector<cpureg*> m_regs;

    void update_regs();

    void fetch_arch();

public:
    target(connection& conn, const string& name, const string& arch,
           target_group& group);
    virtual ~target();

    target() = delete;
    target(const target&) = delete;
    target& operator=(const target&) = delete;

    const char* name() const { return m_name.c_str(); }
    const char* arch() const { return m_arch.c_str(); }

    const target_group& group() const { return m_group; }
    const char* group_name() const { return m_group.name.c_str(); }

    void step();
    void step(size_t steps);

    u64 virt_to_phys(u64 va);

    breakpoint insert_breakpoint(u64 addr);
    void remove_breakpoint(const breakpoint& bp);

    watchpoint insert_watchpoint(u64 base, u64 size, watchpoint_type type);
    void remove_watchpoint(const watchpoint& wp);

    vector<u8> read_vmem(u64 vaddr, size_t size);
    size_t write_vmem(u64 vaddr, const vector<u8>& data);

    vector<u8> read_vmem_ex(u64 vaddr, size_t size,
                            vector<vmem_status>& status);

    vector<symbol> symbols();

    vector<instruction> disassemble(u64 vaddr, size_t count);

    vector<stackframe> stacktrace(size_t limit = 64);

    target_info info();

    vector<u8> read_pmem(u64 paddr, size_t size);
    size_t write_pmem(u64 paddr, const vector<u8>& data);

    u64 get_pc();

    const vector<cpureg*>& regs() { return m_regs; }
    cpureg* find_reg(const string& name);
};

} // namespace vsp

#endif
