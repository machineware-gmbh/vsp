#include <array>
#include <vcml.h>

#include "simple_cpu.h"

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

        clock_.clk.bind(bus_.clk);
        clock_.clk.bind(cpu.clk);
        clock_.clk.bind(mem_.clk);

        reset_.rst.bind(bus_.rst);
        reset_.rst.bind(cpu.rst);
        reset_.rst.bind(mem_.rst);
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
