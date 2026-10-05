/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "simple_gen.h"

simple_gen::simple_gen(const sc_core::sc_module_name& nm):
    vcml::module(nm),
    vcml::serial_host(),
    enabled("enabled", false),
    period("period", sc_core::sc_time(10, sc_core::SC_US)),
    throttle("throttle", 1000),
    serial_tx("serial_tx"),
    serial_rx("serial_rx"),
    led_out("led_out") {
    SC_HAS_PROCESS(simple_gen);
    SC_THREAD(run);
}

void simple_gen::run() {
    if (!enabled)
        return;

    bool states[NUM_LEDS] = {};
    for (size_t tick = 0;; tick++) {
        wait(period);
        if (throttle > 0)
            mwr::usleep(throttle);

        for (char c : mwr::mkstr("tick %zu\n", tick))
            serial_tx.send((mwr::u8)c);

        size_t idx = tick % NUM_LEDS;
        states[idx] = !states[idx];
        led_out[idx] = states[idx];
    }
}
