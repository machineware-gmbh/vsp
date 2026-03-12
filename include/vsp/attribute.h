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

    void set(const string& val);

    template <typename T>
    void set(T val);

    template <typename T>
    void set(const vector<T>& val);
};

template <>
void attribute::set<const char*>(const char* val);

template <>
void attribute::set<bool>(bool val);

template <typename T>
void attribute::set(T val) {
    set(to_string(val));
}

template <typename T>
void attribute::set(const vector<T>& val) {
    MWR_ERROR_ON(val.size() != m_count, "size missmatch");

    if (val.empty())
        return;

    std::stringstream ss;

    ss << val[0];
    for (size_t i = 1; i < val.size(); ++i)
        ss << ',' << val[i];

    set(ss.str());
}

} // namespace vsp

#endif
