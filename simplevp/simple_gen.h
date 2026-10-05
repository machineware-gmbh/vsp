/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef SIMPLE_GEN_H
#define SIMPLE_GEN_H

#include "vcml.h"

class simple_gen : public vcml::module, public vcml::serial_host
{
public:
    static constexpr size_t NUM_LEDS = 4;

    vcml::property<bool> enabled;
    vcml::property<sc_core::sc_time> period;

    // wall-clock microseconds per tick
    vcml::property<mwr::u64> throttle;

    vcml::serial_initiator_socket serial_tx;
    vcml::serial_target_socket serial_rx;
    vcml::gpio_initiator_array<NUM_LEDS> led_out;

    simple_gen(const sc_core::sc_module_name& nm);
    virtual ~simple_gen() = default;
    VCML_KIND(simple_gen);

private:
    void run();
};

#endif
