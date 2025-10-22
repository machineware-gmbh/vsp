/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_COMMON_H
#define VSP_COMMON_H

#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <filesystem>

#include <mwr.h>

namespace vsp {

namespace fs = std::filesystem;

using mwr::i8;
using mwr::i16;
using mwr::i32;
using mwr::i64;

using mwr::u8;
using mwr::u16;
using mwr::u32;
using mwr::u64;

using mwr::socket;
using mwr::mkstr;

using mwr::split;

using std::list;
using std::vector;
using std::unordered_map;

using std::string;
using std::stringstream;
using std::to_string;
using std::ostream;
using std::count;
using std::ifstream;

using std::stoi;
using std::stoull;

using std::cout;
using std::cerr;
using std::cin;
using std::endl;

using std::mutex;
using std::lock_guard;

using std::optional;
using std::nullopt;

using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;
using std::make_unique;

using std::function;
using std::pair;

inline string response_get_error(const optional<vector<string>>& resp) {
    if (!resp.has_value())
        return "no response";
    if (resp.value().empty())
        return "no response";
    if (resp.value()[0] != "E")
        return "no error";
    if (resp.value().size() < 2)
        return "unknown error";
    return resp.value()[1];
}

} // namespace vsp

#endif
