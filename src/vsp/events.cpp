/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsp/events.h"
#include "vsp/module.h"
#include "vsp/payloads.h"

namespace vsp {

static const char* const PROTOCOL_NAMES[VSP_TRACE_PROTOCOL_COUNT] = {
    "UNKNOWN", "TLM",    "GPIO",   "CLK",    "PCI",      "I2C", "LIN", "SPI",
    "SD",      "SERIAL", "SIGNAL", "VIRTIO", "ETHERNET", "CAN", "USB",
};

const char* trace_dir_str(vsp_trace_dir dir) {
    return dir == VSP_TRACE_BW ? "bw" : "fw";
}

string format_time(u64 time_ps) {
    u64 ns = time_ps / 1000;
    return mkstr("%llu.%09llus", (unsigned long long)(ns / 1000000000ull),
                 (unsigned long long)(ns % 1000000000ull));
}

const char* trace_protocol_str(vsp_trace_protocol protocol) {
    if (protocol < 0 || protocol >= VSP_TRACE_PROTOCOL_COUNT)
        return PROTOCOL_NAMES[VSP_TRACE_PROTOCOL_UNKNOWN];
    return PROTOCOL_NAMES[protocol];
}

vsp_trace_protocol trace_protocol_from_str(const char* str) {
    if (str == nullptr)
        return VSP_TRACE_PROTOCOL_UNKNOWN;

    for (int i = VSP_TRACE_PROTOCOL_UNKNOWN + 1; i < VSP_TRACE_PROTOCOL_COUNT;
         i++) {
        if (strcmp(str, PROTOCOL_NAMES[i]) == 0)
            return (vsp_trace_protocol)i;
    }

    return VSP_TRACE_PROTOCOL_UNKNOWN;
}

// field accessors: return false if the field is missing or has another type

template <typename T>
static bool get_uint(const json& obj, const char* key, T& val) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_number_unsigned())
        return false;
    val = (T)it->get<u64>();
    return true;
}

static bool get_bool(const json& obj, const char* key, bool& val) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_boolean())
        return false;
    val = it->get<bool>();
    return true;
}

static bool get_string(const json& obj, const char* key, string& val) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_string())
        return false;
    val = it->get<string>();
    return true;
}

static bool get_bytes(const json& obj, const char* key, vector<u8>& val) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_array())
        return false;

    val.clear();
    for (const auto& byte : *it) {
        if (!byte.is_number_unsigned())
            return false;
        val.push_back((u8)byte.get<u64>());
    }

    return true;
}

bool parse_payload(const json& obj, trace_tlm& tx) {
    if (!obj.is_object() || !get_string(obj, "command", tx.command) ||
        !get_uint(obj, "address", tx.address) ||
        !get_bytes(obj, "data", tx.data) ||
        !get_bytes(obj, "byte_enable", tx.byte_enable) ||
        !get_uint(obj, "streaming_width", tx.streaming_width) ||
        !get_bool(obj, "dmi_allowed", tx.dmi_allowed) ||
        !get_string(obj, "response_status", tx.response)) {
        return false;
    }

    tx.sbi.reset();
    auto sbi = obj.find("sbi");
    if (sbi != obj.end()) {
        trace_tlm_sbi info;
        if (!get_bool(*sbi, "is_debug", info.is_debug) ||
            !get_bool(*sbi, "is_nodmi", info.is_nodmi) ||
            !get_bool(*sbi, "is_sync", info.is_sync) ||
            !get_bool(*sbi, "is_insn", info.is_insn) ||
            !get_bool(*sbi, "is_excl", info.is_excl) ||
            !get_bool(*sbi, "is_lock", info.is_lock) ||
            !get_bool(*sbi, "is_secure", info.is_secure) ||
            !get_string(*sbi, "atype", info.atype) ||
            !get_uint(*sbi, "cpuid", info.cpuid) ||
            !get_uint(*sbi, "privilege", info.privilege) ||
            !get_uint(*sbi, "asid", info.asid)) {
            return false;
        }
        tx.sbi = info;
    }

    return true;
}

bool parse_payload(const json& obj, trace_gpio& tx) {
    if (!obj.is_object() || !get_bool(obj, "state", tx.state))
        return false;

    u32 vector = 0;
    tx.gpio_vector.reset();
    if (get_uint(obj, "vector", vector))
        tx.gpio_vector = vector;
    return true;
}

bool parse_payload(const json& obj, trace_clk& tx) {
    string polarity;
    if (!obj.is_object() || !get_uint(obj, "period", tx.period_ns) ||
        !get_string(obj, "polarity", polarity)) {
        return false;
    }

    auto duty = obj.find("duty_cycle");
    if (duty == obj.end() || !duty->is_number())
        return false;

    tx.posedge = polarity == "posedge";
    tx.duty_cycle = duty->get<double>();
    return true;
}

bool parse_payload(const json& obj, trace_pci& tx) {
    return obj.is_object() && get_string(obj, "command", tx.command) &&
           get_string(obj, "response", tx.response) &&
           get_string(obj, "address_space", tx.address_space) &&
           get_uint(obj, "address", tx.address) &&
           get_bytes(obj, "data", tx.data) && get_bool(obj, "debug", tx.debug);
}

bool parse_payload(const json& obj, trace_i2c& tx) {
    return obj.is_object() && get_string(obj, "command", tx.command) &&
           get_string(obj, "response", tx.response) &&
           get_uint(obj, "data", tx.data);
}

bool parse_payload(const json& obj, trace_lin& tx) {
    return obj.is_object() && get_uint(obj, "linid", tx.linid) &&
           get_bytes(obj, "data", tx.data) &&
           get_string(obj, "status", tx.status);
}

bool parse_payload(const json& obj, trace_spi& tx) {
    return obj.is_object() && get_uint(obj, "mosi", tx.mosi) &&
           get_uint(obj, "miso", tx.miso);
}

bool parse_payload(const json& obj, trace_sd& tx) {
    if (!obj.is_object() || !get_string(obj, "command", tx.command))
        return false;

    tx.opcode.clear();
    tx.argument = 0;
    tx.crc = 0;
    tx.spi = false;
    tx.data.clear();
    tx.status.clear();

    if (tx.command == "SD_CMD" || tx.command == "SD_APP_CMD") {
        return get_string(obj, "opcode", tx.opcode) &&
               get_uint(obj, "argument", tx.argument) &&
               get_uint(obj, "crc", tx.crc) && get_bool(obj, "spi", tx.spi) &&
               get_bytes(obj, "response", tx.data) &&
               get_string(obj, "status", tx.status);
    }

    // data transfers; failed reads carry no data byte
    u8 data = 0;
    if (get_uint(obj, "data", data))
        tx.data.push_back(data);
    get_string(obj, "status", tx.status);
    return true;
}

static bool get_buffers(const json& obj, const char* key,
                        vector<trace_virtio_buffer>& val) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_array())
        return false;

    val.clear();
    for (const auto& buf : *it) {
        trace_virtio_buffer b;
        if (!get_uint(buf, "addr", b.addr) || !get_uint(buf, "size", b.size))
            return false;
        val.push_back(b);
    }

    return true;
}

bool parse_payload(const json& obj, trace_virtio& tx) {
    return obj.is_object() && get_uint(obj, "index", tx.index) &&
           get_buffers(obj, "input_buffers", tx.input_buffers) &&
           get_buffers(obj, "output_buffers", tx.output_buffers) &&
           get_string(obj, "status", tx.status);
}

bool parse_payload(const json& obj, trace_serial& tx) {
    return obj.is_object() && get_uint(obj, "data", tx.data) &&
           get_uint(obj, "bits", tx.bits) && get_uint(obj, "baud", tx.baud) &&
           get_string(obj, "parity", tx.parity) &&
           get_string(obj, "stop", tx.stop);
}

bool parse_payload(const json& obj, trace_signal& tx) {
    if (!obj.is_object())
        return false;

    auto it = obj.find("data");
    if (it == obj.end())
        return false;

    if (it->is_null())
        tx.data = std::monostate();
    else if (it->is_boolean())
        tx.data = it->get<bool>();
    else if (it->is_number_unsigned())
        tx.data = it->get<u64>();
    else if (it->is_number_integer())
        tx.data = it->get<i64>();
    else if (it->is_number_float())
        tx.data = it->get<double>();
    else if (it->is_string())
        tx.data = it->get<string>();
    else
        return false;

    return true;
}

bool parse_payload(const json& obj, trace_ethernet& tx) {
    if (!obj.is_object() || !get_string(obj, "type", tx.type))
        return false;

    // invalid frames only report their type
    tx.source.clear();
    tx.destination.clear();
    tx.data.clear();
    get_string(obj, "sourceaddr", tx.source);
    get_string(obj, "destaddr", tx.destination);
    get_bytes(obj, "data", tx.data);
    return true;
}

bool parse_payload(const json& obj, trace_can& tx) {
    if (!obj.is_object() || !get_string(obj, "type", tx.type) ||
        !get_uint(obj, "id", tx.id) || !get_bool(obj, "xlf", tx.xlf) ||
        !get_bool(obj, "fdf", tx.fdf) || !get_bool(obj, "eff", tx.eff) ||
        !get_bytes(obj, "data", tx.data)) {
        return false;
    }

    // fields that depend on the frame type are false/0 if not present
    tx.sec = tx.rrs = tx.brs = tx.esi = tx.rtr = tx.err = false;
    tx.vcid = tx.sdt = 0;
    tx.af = 0;
    get_bool(obj, "sec", tx.sec);
    get_bool(obj, "rrs", tx.rrs);
    get_uint(obj, "vcid", tx.vcid);
    get_uint(obj, "sdt", tx.sdt);
    get_uint(obj, "af", tx.af);
    get_bool(obj, "brs", tx.brs);
    get_bool(obj, "esi", tx.esi);
    get_bool(obj, "rtr", tx.rtr);
    get_bool(obj, "err", tx.err);
    return true;
}

bool parse_payload(const json& obj, trace_usb& tx) {
    return obj.is_object() && get_string(obj, "token", tx.token) &&
           get_uint(obj, "addr", tx.addr) &&
           get_uint(obj, "endpoint", tx.endpoint) &&
           get_bytes(obj, "data", tx.data) &&
           get_string(obj, "status", tx.status);
}

template <typename T>
static optional<trace_payload> parse_as(const json& tx) {
    T payload;
    if (!parse_payload(tx, payload))
        return std::nullopt;
    return trace_payload(std::move(payload));
}

optional<trace_payload> parse_payload(vsp_trace_protocol protocol,
                                      const string& name, const json& tx) {
    switch (protocol) {
    case VSP_TRACE_PROTOCOL_TLM:
        return parse_as<trace_tlm>(tx);
    case VSP_TRACE_PROTOCOL_GPIO:
        return parse_as<trace_gpio>(tx);
    case VSP_TRACE_PROTOCOL_CLK:
        return parse_as<trace_clk>(tx);
    case VSP_TRACE_PROTOCOL_PCI:
        return parse_as<trace_pci>(tx);
    case VSP_TRACE_PROTOCOL_I2C:
        return parse_as<trace_i2c>(tx);
    case VSP_TRACE_PROTOCOL_LIN:
        return parse_as<trace_lin>(tx);
    case VSP_TRACE_PROTOCOL_SPI:
        return parse_as<trace_spi>(tx);
    case VSP_TRACE_PROTOCOL_SD:
        return parse_as<trace_sd>(tx);
    case VSP_TRACE_PROTOCOL_SERIAL:
        return parse_as<trace_serial>(tx);
    case VSP_TRACE_PROTOCOL_SIGNAL:
        return parse_as<trace_signal>(tx);
    case VSP_TRACE_PROTOCOL_VIRTIO:
        return parse_as<trace_virtio>(tx);
    case VSP_TRACE_PROTOCOL_ETHERNET:
        return parse_as<trace_ethernet>(tx);
    case VSP_TRACE_PROTOCOL_CAN:
        return parse_as<trace_can>(tx);
    case VSP_TRACE_PROTOCOL_USB:
        return parse_as<trace_usb>(tx);
    default:
        return trace_payload(trace_unknown{ name });
    }
}

optional<char> parse_uart(const json& payload) {
    if (!payload.is_string())
        return std::nullopt;

    // json strings are utf-8, the code point of the character is the byte
    const string& s = payload.get_ref<const string&>();
    if (s.size() == 1 && (u8)s[0] < 0x80)
        return s[0];

    if (s.size() == 2 && ((u8)s[0] & 0xe0) == 0xc0 &&
        ((u8)s[1] & 0xc0) == 0x80) {
        u32 cp = (((u8)s[0] & 0x1f) << 6) | ((u8)s[1] & 0x3f);
        if (cp <= 0xff)
            return (char)cp;
    }

    return std::nullopt;
}

// printing

static void print_bytes(ostream& os, const vector<u8>& data) {
    os << "[";
    for (size_t i = 0; i < data.size(); i++)
        os << mkstr(i ? " %02x" : "%02x", data[i]);
    os << "]";
}

ostream& operator<<(ostream& os, const trace_info& info) {
    os << format_time(info.time_ps) << " " << info.port.hierarchy_name()
       << (info.is_request() ? " >>" : " <<");
    if (info.error)
        os << " ERROR";
    return os;
}

static void print(ostream& os, const trace_unknown& tx) {
    os << tx.protocol;
}

static void print(ostream& os, const trace_tlm& tx) {
    os << "TLM " << tx.command
       << mkstr(" @0x%llx ", (unsigned long long)tx.address);
    print_bytes(os, tx.data);
    os << " " << tx.response;
}

static void print(ostream& os, const trace_gpio& tx) {
    os << "GPIO " << (tx.state ? "1" : "0");
    if (tx.gpio_vector)
        os << " vector=" << *tx.gpio_vector;
}

static void print(ostream& os, const trace_clk& tx) {
    os << "CLK ";
    if (tx.period_ns == 0)
        os << "off";
    else
        os << tx.period_ns << "ns " << (tx.posedge ? "posedge" : "negedge")
           << " duty=" << tx.duty_cycle;
}

static void print(ostream& os, const trace_pci& tx) {
    os << "PCI " << tx.command << " " << tx.address_space
       << mkstr(" @0x%llx ", (unsigned long long)tx.address);
    print_bytes(os, tx.data);
    os << " " << tx.response;
}

static void print(ostream& os, const trace_i2c& tx) {
    os << "I2C " << tx.command << mkstr(" 0x%02x ", tx.data) << tx.response;
}

static void print(ostream& os, const trace_lin& tx) {
    os << "LIN id=" << (unsigned int)tx.linid << " ";
    print_bytes(os, tx.data);
    os << " " << tx.status;
}

static void print(ostream& os, const trace_spi& tx) {
    os << mkstr("SPI mosi=0x%02x miso=0x%02x", tx.mosi, tx.miso);
}

static void print(ostream& os, const trace_sd& tx) {
    os << "SD " << tx.command;
    if (!tx.opcode.empty())
        os << " " << tx.opcode << mkstr(" arg=0x%08x", tx.argument);
    os << " ";
    print_bytes(os, tx.data);
    os << " " << tx.status;
}

static void print(ostream& os, const trace_virtio& tx) {
    auto buffers = [&os](const vector<trace_virtio_buffer>& bufs) {
        os << "[";
        for (size_t i = 0; i < bufs.size(); i++) {
            os << mkstr(i ? " 0x%llx:%llu" : "0x%llx:%llu",
                        (unsigned long long)bufs[i].addr,
                        (unsigned long long)bufs[i].size);
        }
        os << "]";
    };

    os << "VIRTIO index=" << tx.index << " in=";
    buffers(tx.input_buffers);
    os << " out=";
    buffers(tx.output_buffers);
    os << " " << tx.status;
}

static void print(ostream& os, const trace_serial& tx) {
    os << mkstr("SERIAL 0x%02x", tx.data);
    if (tx.data >= 0x20 && tx.data < 0x7f)
        os << " '" << (char)tx.data << "'";
    os << " " << tx.bits << tx.parity << tx.stop << " @" << tx.baud;
}

static void print(ostream& os, const trace_signal& tx) {
    os << "SIGNAL ";
    std::visit(
        [&os](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                os << "null";
            else if constexpr (std::is_same_v<T, bool>)
                os << (val ? "true" : "false");
            else
                os << val;
        },
        tx.data);
}

static void print(ostream& os, const trace_ethernet& tx) {
    os << "ETHERNET " << tx.type;
    if (!tx.source.empty())
        os << " " << tx.source << " -> " << tx.destination;
    os << " " << tx.data.size() << " bytes";
}

static void print(ostream& os, const trace_can& tx) {
    os << tx.type << mkstr(" id=0x%x ", tx.id);
    print_bytes(os, tx.data);
}

static void print(ostream& os, const trace_usb& tx) {
    os << "USB " << tx.token << " addr=" << tx.addr << " ep=" << tx.endpoint
       << " ";
    print_bytes(os, tx.data);
    os << " " << tx.status;
}

ostream& operator<<(ostream& os, const trace_payload& tx) {
    std::visit([&os](const auto& payload) { print(os, payload); }, tx);
    return os;
}

} // namespace vsp
