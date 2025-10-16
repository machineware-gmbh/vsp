#include <array>
#include <vcml.h>

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

protected:
    virtual bool read_reg_dbg(size_t idx, void* buf, size_t len) override;
    virtual bool write_reg_dbg(size_t idx, const void*, size_t l) override;
    std::vector<u16> breakpoints_;
    std::vector<u16> watchpoints_;

    std::array<u32, 32> reg_file = { 0 };
    u32 pc = 0;

    u64 cycle_counter = 0;
    u64 retired_instructions = 0;

    u32 fetch_inst(u32 adr);
};

class simple_system : public vcml::system
{
    static constexpr size_t mem_size = 0x7fff;

public:
    simple_system(const sc_core::sc_module_name& nm):
        vcml::system(nm),
        cpu(),
        mem_range_("memory_range", vcml::range(0, mem_size)),
        mem_("memory", mem_size),
        bus_("bus"),
        clock_("clock", 1 * vcml::GHz),
        reset_("reset") {
        bus_.bind(cpu.data);
        bus_.bind(cpu.insn);
        bus_.bind(mem_.in, mem_range_);

        clock_.clk.bind(cpu.clk);
        clock_.clk.bind(bus_.clk);
        clock_.clk.bind(mem_.clk);

        reset_.rst.bind(bus_.rst);
        reset_.rst.bind(mem_.rst);
        reset_.rst.bind(cpu.rst);
    }

    virtual int run() override { return vcml::system::run(); }

public:
    simple_cpu cpu;

    vcml::generic::bus bus_;
    vcml::generic::clock clock_;
    vcml::generic::memory mem_;
    vcml::generic::reset reset_;

    vcml::property<vcml::range> mem_range_;
};

int sc_main(int argc, char** argv) {
    simple_system system("system");
    vcml::debugging::vspserver session("localhost", 4444);
    session.start();
    return 0;
}
