#include <vcml.h>

#include "simple_cpu.h"

class simple_system : public vcml::system
{
    static constexpr size_t m_memsize = 0x7fff;

public:
    simple_system(const sc_core::sc_module_name& nm):
        vcml::system(nm),
        m_cpu("cpu"),
        m_bus("bus"),
        m_clock("clock", 1 * vcml::GHz),
        m_mem("memory", m_memsize),
        m_reset("reset"),
        m_memrange("memory_range", vcml::range(0, m_memsize)) {
        m_bus.bind(m_cpu.data);
        m_bus.bind(m_cpu.insn);
        m_bus.bind(m_mem.in, m_memrange);

        m_clock.clk.bind(m_bus.clk);
        m_clock.clk.bind(m_cpu.clk);
        m_clock.clk.bind(m_mem.clk);

        m_reset.rst.bind(m_bus.rst);
        m_reset.rst.bind(m_cpu.rst);
        m_reset.rst.bind(m_mem.rst);
    }

    virtual int run() override { return vcml::system::run(); }
    virtual const char* version() const override { return "v1.0"; }

private:
    simple_cpu m_cpu;

    vcml::generic::bus m_bus;
    vcml::generic::clock m_clock;
    vcml::generic::memory m_mem;
    vcml::generic::reset m_reset;

    vcml::property<vcml::range> m_memrange;
};

int sc_main(int argc, char** argv) {
    simple_system system("system");
    vcml::debugging::vspserver session("localhost", 4444);
    session.start();
    return 0;
}
