/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VSP_ATTRIBUTE_H
#define VSP_ATTRIBUTE_H

#include "vsp/common.h"
#include "vsp/element.h"

namespace vsp {

class module;
class attribute : public element
{
private:
    string m_type;
    size_t m_count;

public:
    explicit attribute(const string& name, connection& conn, module* parent,
                       const string& type, size_t count);
    virtual ~attribute() = default;

    attribute() = delete;
    attribute(const attribute&) = delete;
    attribute& operator=(const attribute&) = delete;

    const string& type() const;
    size_t count() const;

    optional<vector<string>> get();
    string get_str();
    bool set(const string& val);
    bool set(bool val);
    bool set(int val);
    bool set(long val);
    bool set(long long val);
    bool set(unsigned val);
    bool set(unsigned long val);
    bool set(unsigned long long val);
    bool set(float val);
    bool set(double val);
    bool set(long double val);
};

} // namespace vsp

#endif
