#include "simple_cpu.h"
#include "vcml.h"

class simple_system : public vcml::system
{
public:
    static constexpr size_t MEMSIZE = 0x8000;

    vcml::property<vcml::range> memrange;

    simple_cpu cpu;

    vcml::generic::bus bus;
    vcml::generic::clock clock;
    vcml::generic::memory mem;
    vcml::generic::reset reset;

    simple_system(const sc_core::sc_module_name& nm):
        vcml::system(nm),
        memrange("memrange", { 0, MEMSIZE - 1 }),
        cpu("cpu"),
        bus("bus"),
        clock("clock", 1 * vcml::GHz),
        mem("memory", MEMSIZE),
        reset("reset") {
        bus.bind(cpu.data);
        bus.bind(cpu.insn);
        bus.bind(mem.in, memrange);

        clock.clk.bind(bus.clk);
        clock.clk.bind(cpu.clk);
        clock.clk.bind(mem.clk);

        reset.rst.bind(bus.rst);
        reset.rst.bind(cpu.rst);
        reset.rst.bind(mem.rst);
    }

    virtual const char* version() const override { return "v1.0"; }
};

int sc_main(int argc, char** argv) {
    simple_system system("system");
    return system.run();
}
