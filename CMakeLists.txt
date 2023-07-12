cmake_minimum_required(VERSION 3.20)

project(hikari LANGUAGES CXX VERSION 0.1.1)

set(CMAKE_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SCRIPTS})

add_library(project_options INTERFACE)
include("cmake/ProjectSettings.cmake")
target_set_warnings(project_options INTERFACE)
target_set_options(project_options INTERFACE)

option(BUILD_SHARED_LIBS "Build library as shared library" OFF)

add_subdirectory(lib)

option(HIKARI_BUILD_EXAMPLES "Build code examples" ON)
if (HIKARI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

option(HIKARI_BUILD_DOCS "Build documentation using Doxygen & Sphinx" OFF)
if (HIKARI_BUILD_DOCS)
    add_subdirectory(docs)
endif ()
