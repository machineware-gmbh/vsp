/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_EVENTS_H
#define VSP_EVENTS_H

#include <variant>

#include "vsp/common.h"

namespace vsp {

// led event, published by vcml::gpio::leds whenever an led changes
struct led_event {
    module& leds; // the leds object that changed
    u64 time_ps;
    u64 delta;
    size_t index;
    bool state;
};

using led_handler = function<void(const led_event&)>;

// uart output, published by vcml::serial::terminal for every character it
// receives; consecutive characters are delivered together
struct uart_event {
    module& terminal; // the terminal that received the data
    u64 time_ps;      // time of the first character
    u64 delta;
    string data; // raw bytes
};

using uart_handler = function<void(const uart_event&)>;

// trace event, published by ports, sockets and registers
enum vsp_trace_dir {
    VSP_TRACE_FW = 0,
    VSP_TRACE_BW,
};

enum vsp_trace_protocol {
    VSP_TRACE_PROTOCOL_UNKNOWN = 0,
    VSP_TRACE_PROTOCOL_TLM,
    VSP_TRACE_PROTOCOL_GPIO,
    VSP_TRACE_PROTOCOL_CLK,
    VSP_TRACE_PROTOCOL_PCI,
    VSP_TRACE_PROTOCOL_I2C,
    VSP_TRACE_PROTOCOL_LIN,
    VSP_TRACE_PROTOCOL_SPI,
    VSP_TRACE_PROTOCOL_SD,
    VSP_TRACE_PROTOCOL_SERIAL,
    VSP_TRACE_PROTOCOL_SIGNAL,
    VSP_TRACE_PROTOCOL_VIRTIO,
    VSP_TRACE_PROTOCOL_ETHERNET,
    VSP_TRACE_PROTOCOL_CAN,
    VSP_TRACE_PROTOCOL_USB,
    VSP_TRACE_PROTOCOL_COUNT,
};

const char* trace_dir_str(vsp_trace_dir dir);
const char* trace_protocol_str(vsp_trace_protocol protocol);
vsp_trace_protocol trace_protocol_from_str(const char* str);

// common part of all trace events
struct trace_info {
    module& port; // socket, port or register that saw the transaction
    u64 time_ps;  // includes the local time offset of the sender
    u64 delta;
    vsp_trace_dir dir;
    bool error;

    bool is_request() const { return dir == VSP_TRACE_FW; }
    bool is_response() const { return dir == VSP_TRACE_BW; }
};

struct trace_tlm_sbi {
    bool is_debug;
    bool is_nodmi;
    bool is_sync;
    bool is_insn;
    bool is_excl;
    bool is_lock;
    bool is_secure;
    string atype; // "untranslated", "translated" or "tx-req"
    u64 cpuid;
    u64 privilege;
    u64 asid;
};

struct trace_tlm {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_TLM;
    string command; // "READ", "WRITE", "IGNORE"
    u64 address;
    vector<u8> data;
    vector<u8> byte_enable;
    u32 streaming_width;
    bool dmi_allowed;
    string response; // e.g. "TLM_OK_RESPONSE"
    optional<trace_tlm_sbi> sbi;

    bool is_read() const { return command == "READ"; }
    bool is_write() const { return command == "WRITE"; }
};

struct trace_gpio {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_GPIO;
    bool state;
    optional<u32> gpio_vector; // "vector" in json
};

struct trace_clk {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_CLK;
    u64 period_ns; // 0 means the clock is off
    bool posedge;
    double duty_cycle;
};

struct trace_pci {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_PCI;
    string command;
    string response;
    string address_space;
    u64 address;
    vector<u8> data;
    bool debug;
};

struct trace_i2c {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_I2C;
    string command;
    string response;
    u8 data;
};

struct trace_lin {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_LIN;
    u8 linid;
    vector<u8> data;
    string status;
};

struct trace_spi {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_SPI;
    u8 mosi;
    u8 miso;
};

// sd commands ("SD_CMD", "SD_APP_CMD") and data ("SD_DATA_READ/WRITE")
struct trace_sd {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_SD;
    string command;
    string opcode;   // commands only
    u32 argument;    // commands only
    u8 crc;          // commands only
    bool spi;        // commands only
    vector<u8> data; // command response bytes, or the data byte
    string status;
};

struct trace_virtio_buffer {
    u64 addr;
    u64 size;
};

struct trace_virtio {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_VIRTIO;
    u32 index;
    vector<trace_virtio_buffer> input_buffers;
    vector<trace_virtio_buffer> output_buffers;
    string status;
};

struct trace_serial {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_SERIAL;
    u32 data;
    u32 bits;
    u32 baud;
    string parity;
    string stop;
};

struct trace_signal {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_SIGNAL;
    // null for NaN/inf, string for types without a json equivalent
    std::variant<std::monostate, bool, u64, i64, double, string> data;
};

struct trace_ethernet {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_ETHERNET;
    string type;
    string source;      // empty for invalid frames
    string destination; // empty for invalid frames
    vector<u8> data;
};

struct trace_can {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_CAN;
    string type; // "CAN", "CAN_FD" or "CAN_XL"
    u32 id;
    bool xlf;
    bool fdf;
    bool eff;
    bool sec; // CAN_XL only
    bool rrs; // CAN_XL only
    u8 vcid;  // CAN_XL only
    u8 sdt;   // CAN_XL only
    u32 af;   // CAN_XL only
    bool brs; // CAN_FD only
    bool esi; // CAN_FD only
    bool rtr; // CAN only
    bool err; // CAN only
    vector<u8> data;
};

struct trace_usb {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_USB;
    string token;
    u32 addr;
    u32 endpoint;
    vector<u8> data;
    string status;
};

// trace event of a protocol this library does not know yet
struct trace_unknown {
    static constexpr vsp_trace_protocol PROTOCOL = VSP_TRACE_PROTOCOL_UNKNOWN;
    string protocol;
};

using trace_payload = std::variant<
    trace_unknown, trace_tlm, trace_gpio, trace_clk, trace_pci, trace_i2c,
    trace_lin, trace_spi, trace_sd, trace_virtio, trace_serial, trace_signal,
    trace_ethernet, trace_can, trace_usb>;

using trace_handler = function<void(const trace_info& info,
                                    const trace_payload& tx)>;

// one-line summaries, e.g. for printing traces on a console
ostream& operator<<(ostream& os, const trace_info& info);
ostream& operator<<(ostream& os, const trace_payload& tx);

} // namespace vsp

#endif
