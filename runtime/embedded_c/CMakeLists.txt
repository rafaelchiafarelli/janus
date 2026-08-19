cmake_minimum_required(VERSION 3.16)
project(janus_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(janus_runtime STATIC src/janus_runtime.c)
target_include_directories(janus_runtime PUBLIC include)
target_compile_options(janus_runtime PRIVATE -Wall -Wextra)

option(JANUS_RUNTIME_BUILD_TESTS "Build janus_runtime's own host-side tests" ON)
if(JANUS_RUNTIME_BUILD_TESTS)
    add_library(janus_host_mock STATIC host_mock/mock_driver.c)
    target_include_directories(janus_host_mock PUBLIC host_mock)
    target_link_libraries(janus_host_mock PUBLIC janus_runtime)

    add_executable(janus_runtime_tests tests/test_runtime.c)
    target_link_libraries(janus_runtime_tests PRIVATE janus_host_mock janus_runtime)
    target_compile_options(janus_runtime_tests PRIVATE -Wall -Wextra)

    enable_testing()
    add_test(NAME janus_runtime_tests COMMAND janus_runtime_tests)
endif()
