 ##############################################################################
 #                                                                            #
 # Copyright (C) 2025 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

if(NOT TARGET vcml)
    set(VCML_USE_SDL2 OFF CACHE BOOL "Disable SDL2" FORCE)
    set(VCML_USE_SLIRP OFF CACHE BOOL "Disable SLIRP" FORCE)
    set(VCML_USE_TAP OFF CACHE BOOL "Disable TAP networking" FORCE)
    set(VCML_USE_LUA OFF CACHE BOOL "Disable LUA scripting" FORCE)
    set(VCML_USE_SOCKETCAN OFF CACHE BOOL "Disable CAN sockets" FORCE)
    set(VCML_USE_USB OFF CACHE BOOL "Disable LibUSB" FORCE)
    find_github_repo(vcml "machineware-gmbh/vcml")
endif()

add_executable(simplevp simple_vp.cpp simple_cpu.cpp)
target_compile_options(simplevp PRIVATE ${MWR_COMPILER_WARN_FLAGS})
target_link_libraries(simplevp PRIVATE vcml)
