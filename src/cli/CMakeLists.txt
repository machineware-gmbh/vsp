 ##############################################################################
 #                                                                            #
 # Copyright (C) 2025 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

add_executable(vsp-cli
    ${src}/cli/cli.cpp
    ${src}/cli/main.cpp
    ${src}/cli/session.cpp
    ${src}/cli/startup.cpp)

target_compile_options(vsp-cli PRIVATE ${MWR_COMPILER_WARN_FLAGS})
target_compile_features(vsp-cli PUBLIC cxx_std_17)

target_include_directories(vsp-cli PRIVATE ${src})
target_include_directories(vsp-cli PRIVATE ${gen})

set_target_properties(vsp-cli PROPERTIES DEBUG_POSTFIX "d")
set_target_properties(vsp-cli PROPERTIES CXX_CLANG_TIDY "${VSP_LINTER}")
set_target_properties(vsp-cli PROPERTIES VERSION "${VSP_VERSION}")
set_target_properties(vsp-cli PROPERTIES SOVERSION "${VSP_VERSION_MAJOR}")

target_link_libraries(vsp-cli PUBLIC vsp)

install(TARGETS vsp-cli DESTINATION bin)
