cmake_minimum_required(VERSION 3.16)
project(janus_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(janus_runtime STATIC
    src/janus_runtime.c
    src/janus_input_touch.c
)
target_include_directories(janus_runtime PUBLIC include)
target_compile_options(janus_runtime PRIVATE -Wall -Wextra)

option(JANUS_RUNTIME_BUILD_TESTS "Build janus_runtime's own host-side tests" ON)
if(JANUS_RUNTIME_BUILD_TESTS)
    add_library(janus_host_mock STATIC
        host_mock/mock_driver.c
        host_mock/mock_touch.c
    )
    target_include_directories(janus_host_mock PUBLIC host_mock)
    target_link_libraries(janus_host_mock PUBLIC janus_runtime)

    # Link order matters here: janus_runtime is the consumer of the
    # driver-contract symbols (draw_area_sync, janus_touch_poll, ...)
    # that janus_host_mock provides, so janus_runtime must come first —
    # a single-pass linker only rescans an earlier static archive for
    # symbols that became newly-needed after it, never the reverse.
    add_executable(janus_runtime_tests tests/test_runtime.c)
    target_link_libraries(janus_runtime_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_runtime_tests PRIVATE -Wall -Wextra)

    add_executable(janus_input_touch_tests tests/test_input_touch.c)
    target_link_libraries(janus_input_touch_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_touch_tests PRIVATE -Wall -Wextra)

    enable_testing()
    add_test(NAME janus_runtime_tests COMMAND janus_runtime_tests)
    add_test(NAME janus_input_touch_tests COMMAND janus_input_touch_tests)
endif()
