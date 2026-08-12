/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "simple_cpu.h"
#include "vcml.h"

class simple_system : public vcml::system
{
public:
    static constexpr size_t MEMSIZE = 0x8000;

    vcml::property<vcml::range> memrange;
    vcml::property<size_t> num_cpus;

    std::vector<simple_cpu*> cpus;

    vcml::generic::bus bus;
    vcml::generic::clock clock;
    vcml::generic::memory mem;
    vcml::generic::reset reset;

    simple_system(const sc_core::sc_module_name& nm):
        vcml::system(nm),
        memrange("memrange", { 0, MEMSIZE - 1 }),
        num_cpus("num_cpus", 2),
        cpus(),
        bus("bus"),
        clock("clock", 1 * vcml::GHz),
        mem("memory", MEMSIZE),
        reset("reset") {
        bus.bind(mem.in, memrange);

        clock.clk.bind(bus.clk);
        clock.clk.bind(mem.clk);

        reset.rst.bind(bus.rst);
        reset.rst.bind(mem.rst);

        for (size_t i = 0; i < num_cpus; i++) {
            cpus.push_back(new simple_cpu(mwr::mkstr("cpu%zu", i).c_str()));
            cpus[i]->set_target_group(std::string("processors"));

            bus.bind(cpus[i]->data);
            bus.bind(cpus[i]->insn);

            clock.clk.bind(cpus[i]->clk);
            reset.rst.bind(cpus[i]->rst);
        }
    }

    ~simple_system() {
        for (auto *cpu : cpus) {
            if (cpu) {
                delete cpu;
                cpu = nullptr;
            }
        }
    }

    virtual const char* version() const override { return "v1.0"; }
};

int sc_main(int argc, char** argv) {
    simple_system system("system");
    return system.run();
}
