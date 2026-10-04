/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_PAYLOADS_H
#define VSP_PAYLOADS_H

#include <nlohmann/json.hpp>

#include "vsp/common.h"
#include "vsp/events.h"

namespace vsp {

using json = nlohmann::json;

// parse the "tx" part of a trace event; return false if it does not match
bool parse_payload(const json& tx, trace_tlm& out);
bool parse_payload(const json& tx, trace_gpio& out);
bool parse_payload(const json& tx, trace_clk& out);
bool parse_payload(const json& tx, trace_pci& out);
bool parse_payload(const json& tx, trace_i2c& out);
bool parse_payload(const json& tx, trace_lin& out);
bool parse_payload(const json& tx, trace_spi& out);
bool parse_payload(const json& tx, trace_sd& out);
bool parse_payload(const json& tx, trace_virtio& out);
bool parse_payload(const json& tx, trace_serial& out);
bool parse_payload(const json& tx, trace_signal& out);
bool parse_payload(const json& tx, trace_ethernet& out);
bool parse_payload(const json& tx, trace_can& out);
bool parse_payload(const json& tx, trace_usb& out);

// parses tx according to protocol, name is used for unknown protocols
optional<trace_payload> parse_payload(vsp_trace_protocol protocol,
                                      const string& name, const json& tx);

// payload of uart events: one character, bytes >= 0x80 as \u00XX
optional<char> parse_uart(const json& payload);

} // namespace vsp

#endif
