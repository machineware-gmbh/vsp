/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_CLI_CLI_H
#define VSP_CLI_CLI_H

#include "vsp/cmn.h"

namespace vsp {

class cli
{
private:
    using handler = function<bool(const string&)>;

    unordered_map<string, pair<handler, string>> m_handler;
    unordered_map<string, string> m_alias;

    bool handle_help(const string& args);
    bool print();

protected:
    template <class T>
    void register_handler(bool (T::*h)(const string&), const string& cmd,
                          const string& desc, const string& alias = "");

    virtual string prompt() const;
    virtual bool before_run();

public:
    cli();
    virtual ~cli() = default;

    cli(const cli&) = delete;
    cli& operator=(const cli&) = delete;

    int run();
};

template <class T>
void cli::register_handler(bool (T::*h)(const string&), const string& cmd,
                           const string& desc, const string& alias) {
    m_handler[cmd] = { bind(h, static_cast<T*>(this), std::placeholders::_1),
                       desc };
    if (!alias.empty())
        m_alias[alias] = cmd;
}

} // namespace vsp

#endif
