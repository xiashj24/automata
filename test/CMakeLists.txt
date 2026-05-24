enable_testing()

add_executable(unit_tests math.test.cpp param.test.cpp graph.test.cpp)

target_compile_features(unit_tests PRIVATE cxx_std_${CMAKE_CXX_STANDARD})

target_include_directories(unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain automata::automata)

myproject_set_project_warnings(unit_tests ${AUTOMATA_WARNINGS_AS_ERRORS})

# Catch2's TEST_CASE macro uses __COUNTER__, flagged as C2y extension in Clang 22+
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(unit_tests PRIVATE -Wno-c2y-extensions)
endif()

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

catch_discover_tests(unit_tests DISCOVERY_MODE POST_BUILD)
