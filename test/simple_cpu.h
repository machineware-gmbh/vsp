#ifndef SIMPLE_CPU_H
#define SIMPLE_CPU_H

#include <array>
#include <vector>

#include "vcml.h"

class simple_cpu : public vcml::processor
{
public:
    using i8 = mwr::i8;
    using i16 = mwr::i16;
    using i32 = mwr::i32;
    using i64 = mwr::i64;

    using u8 = mwr::u8;
    using u16 = mwr::u16;
    using u32 = mwr::u32;
    using u64 = mwr::u64;

    using string = mwr::string;

    using inst_t = mwr::u32;
    using reg_t = mwr::u32;

    vcml::property<u64> wait_per_inst;

    vcml::property<bool> bool_property;
    vcml::property<i32> i32_property;
    vcml::property<i64> i64_property;
    vcml::property<u32> u32_property;
    vcml::property<u64> u64_property;
    vcml::property<float> float_property;
    vcml::property<double> double_property;
    vcml::property<long double> long_double_property;
    vcml::property<std::vector<i32>> i32_vector_property;
    vcml::property<std::vector<string>> string_vector_property;

    simple_cpu(const sc_core::sc_module_name& nm);

    virtual u64 cycle_count() const override;

    virtual u64 program_counter() override;
    virtual u64 stack_pointer() override;

    virtual void reset() override;

    virtual void simulate(size_t cycles) override;

    virtual bool insert_breakpoint(u64 addr) override;
    virtual bool remove_breakpoint(u64 addr) override;

    virtual bool insert_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) override;
    virtual bool remove_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) override;

    virtual bool read_reg_dbg(size_t idx, void* buf, size_t len) override;
    virtual bool write_reg_dbg(size_t idx, const void*, size_t l) override;

    virtual bool virt_to_phys(u64 vaddr, u64& paddr) override;

private:
    std::vector<u32> m_breakpoints;
    std::vector<std::tuple<vcml::range, vcml::vcml_access>> m_watchpoints;

    enum stop_reason {
        HIT_BREAKPOINT,
        HIT_RWATCHPOINT,
        HIT_WWATCHPOINT,
    };

    struct cpu_exception {
        cpu_exception(stop_reason reason): reason(reason) {}
        stop_reason reason;
    };

    std::array<u32, 32> m_reg_file = { 0 };
    u32 m_pc;

    u64 m_num_cycles;

    void exec_inst();

    u32 mem_read(u32 adr, bool trigger_watchpoints = false);
    void mem_write(u32 adr, reg_t dat);
};

#endif
