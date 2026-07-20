cmake_minimum_required(VERSION 3.25)

project(automata LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/get_cpm.cmake)

# Header-only; DOWNLOAD_ONLY sidesteps its CMakeLists entirely.
CPMAddPackage(
  NAME
  miniaudio
  GITHUB_REPOSITORY
  mackron/miniaudio
  GIT_TAG
  0.11.25
  DOWNLOAD_ONLY
  YES)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set(AUTOMATA_WARNINGS -Wall -Wextra)
elseif(MSVC)
  set(AUTOMATA_WARNINGS /W4)
endif()

# ── Library ───────────────────────────────────────────────────────────────────
# Consumers (tests, host, tools, patch modules) get the repo root as include
# path: #include "automata/core/hash.hpp"
add_library(automata STATIC
  automata/core/assert.cpp
  automata/engine/graph.cpp
  automata/engine/offline.cpp
  automata/engine/wav.cpp
)
add_library(automata::automata ALIAS automata)
target_include_directories(automata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(automata SYSTEM PRIVATE "${miniaudio_SOURCE_DIR}")
target_compile_features(automata PUBLIC cxx_std_23)
target_compile_options(automata PRIVATE ${AUTOMATA_WARNINGS})

# ── Tests ─────────────────────────────────────────────────────────────────────
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

CPMAddPackage("gh:catchorg/Catch2@3.8.1")
find_package(Threads REQUIRED)

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
include(Catch)

# Any file matching *.test.cpp under automata/ is picked up automatically as
# its own test executable — just drop a new file in.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/automata/*.test.cpp"
)

foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
  # automata/core/ring_buffer.test.cpp -> target "automata-core-ring_buffer"
  file(RELATIVE_PATH TEST_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${TEST_SOURCE}")
  string(REPLACE ".test.cpp" "" TEST_TARGET "${TEST_REL}")
  string(REPLACE "/" "-" TEST_TARGET "${TEST_TARGET}")

  add_executable("${TEST_TARGET}" "${TEST_SOURCE}")
  target_link_libraries("${TEST_TARGET}" PRIVATE automata::automata
    Catch2::Catch2WithMain Threads::Threads)
  target_compile_options("${TEST_TARGET}" PRIVATE ${AUTOMATA_WARNINGS})

  catch_discover_tests("${TEST_TARGET}")
endforeach()
