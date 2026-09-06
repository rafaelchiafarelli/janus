cmake_minimum_required(VERSION 3.16)
project(janus_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(janus_runtime STATIC
    src/janus_runtime.c
    src/janus_input_touch.c
    src/janus_input_focus.c
    src/janus_font.c
)
target_include_directories(janus_runtime PUBLIC include)
target_compile_options(janus_runtime PRIVATE -Wall -Wextra)

# channel_icons task 4: force the baked-image `.janus_img` section past
# `.text` so near-read flash data (fonts, descriptors, strings) stays
# below AVR's 64 KiB window. INTERFACE so it reaches the consumer's link
# with no edit on their side. Only for an AVR cross-build — `-Wl,-T` is a
# GNU-ld extension a host linker (notably macOS ld64) would reject, and
# the host suites don't need it (the section is empty off-AVR anyway).
if(CMAKE_C_COMPILER MATCHES "avr")
    target_link_options(janus_runtime INTERFACE
        "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/janus_img.ld")
endif()

option(JANUS_RUNTIME_BUILD_TESTS "Build janus_runtime's own host-side tests" ON)
if(JANUS_RUNTIME_BUILD_TESTS)
    # The host test suite exercises the polled/async render path
    # (test_render_async.c), which janus_runtime.c compiles only under
    # JANUS_RENDER_NONBLOCKING (a generated project gets this from its
    # vendored janus_render_config.gen.h; see janus_runtime.h). PUBLIC so
    # every test target that links janus_runtime sees the matching
    # declarations. The *blocking* build — where none of that path is
    # linked — is checked separately by scripts/avr_gate.sh, which
    # compiles this library with the macro absent.
    target_compile_definitions(janus_runtime PUBLIC JANUS_RENDER_NONBLOCKING)

    add_library(janus_host_mock STATIC
        host_mock/mock_driver.c
        host_mock/mock_touch.c
        host_mock/mock_encoder.c
        host_mock/mock_buttons.c
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

    add_executable(janus_input_focus_tests tests/test_input_focus.c)
    target_link_libraries(janus_input_focus_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_focus_tests PRIVATE -Wall -Wextra)

    add_executable(janus_input_encoder_tests tests/test_input_encoder.c)
    target_link_libraries(janus_input_encoder_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_encoder_tests PRIVATE -Wall -Wextra)

    add_executable(janus_input_buttons_tests tests/test_input_buttons.c)
    target_link_libraries(janus_input_buttons_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_buttons_tests PRIVATE -Wall -Wextra)

    add_executable(janus_font_tests tests/test_font.c)
    target_link_libraries(janus_font_tests PRIVATE janus_runtime)
    target_compile_options(janus_font_tests PRIVATE -Wall -Wextra)

    add_executable(janus_render_async_tests tests/test_render_async.c)
    target_link_libraries(janus_render_async_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_render_async_tests PRIVATE -Wall -Wextra)

    enable_testing()
    add_test(NAME janus_runtime_tests COMMAND janus_runtime_tests)
    add_test(NAME janus_input_touch_tests COMMAND janus_input_touch_tests)
    add_test(NAME janus_input_focus_tests COMMAND janus_input_focus_tests)
    add_test(NAME janus_input_encoder_tests COMMAND janus_input_encoder_tests)
    add_test(NAME janus_input_buttons_tests COMMAND janus_input_buttons_tests)
    add_test(NAME janus_font_tests COMMAND janus_font_tests)
    add_test(NAME janus_render_async_tests COMMAND janus_render_async_tests)
endif()
