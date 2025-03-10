/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef CLI_COMMON_H
#define CLI_COMMON_H

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

namespace cli {

using std::string;
using std::to_string;
using std::stringstream;

using std::stoi;
using std::stoull;

using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;
using std::make_unique;

using std::function;
using std::pair;

using std::unordered_map;
using std::vector;

using std::cout;
using std::cin;
using std::endl;

} // namespace cli

#endif
