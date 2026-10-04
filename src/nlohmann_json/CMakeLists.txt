 ##############################################################################
 #                                                                            #
 # Copyright (C) 2026 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

# only used privately by libvsp, its headers are not installed; the target
# name avoids clashes with a nlohmann_json target of a parent project
add_library(vsp_nlohmann_json INTERFACE)

if (CMAKE_VERSION GREATER_EQUAL "3.19")
    set_target_properties(vsp_nlohmann_json PROPERTIES VERSION "3.12.0")
endif()

target_include_directories(vsp_nlohmann_json SYSTEM INTERFACE
                           ${CMAKE_CURRENT_SOURCE_DIR}/include)
install(FILES LICENSE.MIT DESTINATION . RENAME LICENSE.nlohmann-json)
