#include <array>
#include <vcml.h>

using i8 = mwr::i8;
using i16 = mwr::i16;
using i32 = mwr::i32;
using i64 = mwr::i64;

using u8 = mwr::u8;
using u16 = mwr::u16;
using u32 = mwr::u32;
using u64 = mwr::u64;

struct simple_cpu : public vcml::processor {
    static constexpr size_t inst_size = 4;

    simple_cpu();

    virtual mwr::u64 cycle_count() const override;
    virtual void reset() override;
    virtual void simulate(size_t cycles) override;
    virtual vcml::u64 program_counter() override;
    virtual vcml::u64 stack_pointer() override;
    virtual bool insert_breakpoint(vcml::u64 addr) override;
    virtual bool remove_breakpoint(vcml::u64 addr) override;
    virtual bool insert_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) override;
    virtual bool remove_watchpoint(const vcml::range& addr,
                                   vcml::vcml_access prot) override;
    virtual bool virt_to_phys(u64 vaddr, u64& paddr) override;

protected:
    virtual bool read_reg_dbg(size_t idx, void* buf, size_t len) override;
    virtual bool write_reg_dbg(size_t idx, const void*, size_t l) override;
    std::vector<u32> breakpoints_;
    std::vector<std::tuple<vcml::range, vcml::vcml_access>> watchpoints_;

    std::array<u32, 32> reg_file = { 0 };
    u32 pc = 0;

    u64 cycle_counter = 0;
    u64 retired_instructions = 0;

    void exec_inst(u32 inst);
    u32 mem_read(u32 adr, bool trigger_watchpoints = false);
    void mem_write(u32 adr, u32 dat);
};
