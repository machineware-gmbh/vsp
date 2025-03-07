 ##############################################################################
 #                                                                            #
 # Copyright (C) 2025 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

add_library(pugixml INTERFACE)

if (CMAKE_VERSION GREATER_EQUAL "3.19")
    set_target_properties(pugixml PROPERTIES VERSION "1.15")
endif()

target_include_directories(pugixml SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
install(FILES LICENSE.md DESTINATION . RENAME LICENSE-pugixml.md)
