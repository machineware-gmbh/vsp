 ##############################################################################
 #                                                                            #
 # Copyright (C) 2025 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

find_github_repo(vcml "machineware-gmbh/vcml")
add_executable(simplevp simple_vp.cpp simple_cpu.cpp)
target_compile_options(simplevp PRIVATE ${MWR_COMPILER_WARN_FLAGS})
target_link_libraries(simplevp PRIVATE vcml)
