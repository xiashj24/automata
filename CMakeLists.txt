cmake_minimum_required(VERSION 3.27)

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

project(automata LANGUAGES CXX)

option(AUTOMATA_ENABLE_CACHE "Enable ccache" OFF)
option(AUTOMATA_BUILD_TESTS "Build tests" OFF)
option(AUTOMATA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(AUTOMATA_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(AUTOMATA_ENABLE_CPPCHECK "Enable cppcheck" OFF)
option(AUTOMATA_ENABLE_CPPLINT "Enable cpplint" OFF)
option(AUTOMATA_ENABLE_INCLUDE_WHAT_YOU_USE "Enable include-what-you-use" OFF)
option(AUTOMATA_ENABLE_LINK_WHAT_YOU_USE "Enable link-what-you-use" OFF)
option(AUTOMATA_ENABLE_COVERAGE "Enable coverage" OFF)

# MARK: dependencies
set(CPM_SOURCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/.cpm-cache")
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)

include(cmake/get_cpm.cmake)

cpmaddpackage(
  NAME
  Catch2
  GITHUB_REPOSITORY
  catchorg/Catch2
  VERSION
  3.14.0
  SYSTEM
  YES)

cpmaddpackage(
  NAME
  miniaudio
  GITHUB_REPOSITORY
  mackron/miniaudio
  GIT_TAG
  0.11.25
  SYSTEM
  YES)

# MARK: library interface
add_library(automata STATIC src/signal.cpp)
add_library(automata::automata ALIAS automata)
target_include_directories(automata PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(automata PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

# MARK: tooling
if(AUTOMATA_ENABLE_CACHE)
  include(cmake/Cache.cmake)
  myproject_enable_cache()
endif()

include(cmake/CompilerWarnings.cmake)

include(cmake/StaticAnalyzers.cmake)
if(AUTOMATA_ENABLE_CLANG_TIDY)
  myproject_enable_clang_tidy(automata ${AUTOMATA_WARNINGS_AS_ERRORS})
endif()

if(AUTOMATA_ENABLE_CPPCHECK)
  myproject_enable_cppcheck(${AUTOMATA_WARNINGS_AS_ERRORS} "")
endif()

if(AUTOMATA_ENABLE_CPPLINT)
  myproject_enable_cpplint()
endif()

if(AUTOMATA_ENABLE_INCLUDE_WHAT_YOU_USE)
  myproject_enable_include_what_you_use()
endif()

if(AUTOMATA_ENABLE_LINK_WHAT_YOU_USE)
  myproject_enable_link_what_you_use()
endif()

if(AUTOMATA_ENABLE_COVERAGE)
  include(cmake/Tests.cmake)
  myproject_enable_coverage(automata)
endif()

include(CTest)

# MARK: automata-render executable
add_executable(automata-render src/automata_render.cpp)
target_link_libraries(automata-render PRIVATE automata::automata)
target_include_directories(automata-render PRIVATE src)
target_compile_definitions(automata-render PRIVATE AUTOMATA_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(automata-audio src/main.cpp)
target_link_libraries(automata-audio PRIVATE automata::automata miniaudio)

if(AUTOMATA_BUILD_TESTS)
  message(STATUS "Building Tests.")
  add_subdirectory(test)
endif()
