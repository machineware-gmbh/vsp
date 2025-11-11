 ##############################################################################
 #                                                                            #
 # Copyright (C) 2025 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

cmake_minimum_required(VERSION 3.16)
project(vsp VERSION 2025.11.11 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(FindGithubRepo)
include(GenVersionInfo)
include(Sanitizer)

find_package(Git REQUIRED)
find_github_repo(mwr "machineware-gmbh/mwr")

add_subdirectory(src/pugixml)

option(VSP_TESTS "Build unit tests" OFF)
option(VSP_COVERAGE "Generate coverage data" OFF)
set(VSP_LINTER "" CACHE STRING "Code linter to use")
option(VSP_CLI "Build the CLI application" OFF)

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(inc ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(gen ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(${src}/vsp/version.h.in
               ${gen}/vsp/version.h @ONLY)

add_library(vsp STATIC
    ${src}/vsp/attribute.cpp
    ${src}/vsp/command.cpp
    ${src}/vsp/connection.cpp
    ${src}/vsp/cpureg.cpp
    ${src}/vsp/element.cpp
    ${src}/vsp/module.cpp
    ${src}/vsp/session.cpp
    ${src}/vsp/target.cpp)

target_compile_options(vsp PRIVATE ${MWR_COMPILER_WARN_FLAGS})
target_compile_features(vsp PUBLIC cxx_std_17)

target_include_directories(vsp PUBLIC ${inc})
target_include_directories(vsp PRIVATE ${src})
target_include_directories(vsp PRIVATE ${gen})

target_link_libraries(vsp PUBLIC mwr)
target_link_libraries(vsp PRIVATE pugixml)

set_target_properties(vsp PROPERTIES DEBUG_POSTFIX "d")
set_target_properties(vsp PROPERTIES CXX_CLANG_TIDY "${VSP_LINTER}")
set_target_properties(vsp PROPERTIES VERSION "${VSP_VERSION}")
set_target_properties(vsp PROPERTIES SOVERSION "${VSP_VERSION_MAJOR}")

if(VSP_COVERAGE)
    target_compile_options(vsp PUBLIC --coverage)
    target_link_libraries(vsp PUBLIC -lgcov)
endif()

if(VSP_TESTS)
    message(STATUS "Building tests")
    enable_testing()
    add_subdirectory(test)
endif()

if(VSP_CLI)
    message(STATUS "Building CLI")
    add_subdirectory(src/cli)
endif()

install(TARGETS vsp DESTINATION lib)
install(FILES LICENSE DESTINATION .)
install(DIRECTORY ${inc}/ DESTINATION include)
install(DIRECTORY ${gen}/ DESTINATION include)
