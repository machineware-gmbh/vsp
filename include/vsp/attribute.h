/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
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
    attribute(const string& name, connection& conn, module* parent,
              const string& type, size_t count);
    virtual ~attribute() = default;

    attribute() = delete;
    attribute(const attribute&) = delete;
    attribute& operator=(const attribute&) = delete;

    const string& type() const;
    size_t count() const;

    vector<string> get();
    string get_str();

    void set(const string& val);
    void set(const char* val);
    void set(bool val);

    template <typename T>
    void set(T val);

    template <typename T>
    void set(const vector<T>& val);

    void set(const vector<string>& val);
};

inline void attribute::set(const string& val) {
    vector<string> vec = { val };
    set(vec);
}

inline void attribute::set(const char* val) {
    set(string(val));
}

inline void attribute::set(bool val) {
    set(val ? "true" : "false");
}

template <typename T>
inline void attribute::set(T val) {
    set(to_string(val));
}

template <typename T>
inline void attribute::set(const vector<T>& val) {
    vector<string> vec;
    vec.reserve(m_count);
    for (const auto& v : val)
        vec.push_back(to_string(v));
    set(vec);
}

inline void attribute::set(const vector<string>& val) {
    MWR_ERROR_ON(val.size() != m_count, "size mismatch");
    vector<string> cmd = { "seta", hierarchy_name() };
    cmd.insert(cmd.end(), val.begin(), val.end());
    m_conn.command(cmd);
}

} // namespace vsp

#endif
