/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vsp/element.h"

#include "vsp/module.h"

namespace vsp {

element::element(const string& name, connection& conn, module* parent):
    m_conn(conn), m_parent(parent), m_name(name) {
}

const char* element::name() const {
    return m_name.c_str();
}

string element::hierarchy_name() const {
    if (!m_parent)
        return "";

    if (!m_parent->m_parent)
        return name();

    return m_parent->hierarchy_name() + "." + name();
}

} // namespace vsp
