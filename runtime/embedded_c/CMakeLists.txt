cmake_minimum_required(VERSION 3.16)
project(janus_runtime C)

# Warning flags: GCC/Clang spelling everywhere except MSVC, where -Wextra is
# a hard D8021 error, not a warning.
set(JANUS_WARN_FLAGS
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wextra>)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(janus_runtime STATIC
    src/janus_runtime.c
    src/janus_input_touch.c
    src/janus_input_focus.c
    src/janus_font.c
    src/janus_bound.c
    src/janus_format.c
    src/janus_remote.c
)
target_include_directories(janus_runtime PUBLIC include)
target_compile_options(janus_runtime PRIVATE ${JANUS_WARN_FLAGS})

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
    target_compile_options(janus_runtime_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_input_touch_tests tests/test_input_touch.c)
    target_link_libraries(janus_input_touch_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_touch_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_input_focus_tests tests/test_input_focus.c)
    target_link_libraries(janus_input_focus_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_focus_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_input_encoder_tests tests/test_input_encoder.c)
    target_link_libraries(janus_input_encoder_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_encoder_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_input_buttons_tests tests/test_input_buttons.c)
    target_link_libraries(janus_input_buttons_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_input_buttons_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_nav_focus_tests tests/test_nav_focus.c)
    target_link_libraries(janus_nav_focus_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_nav_focus_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_font_tests tests/test_font.c)
    target_link_libraries(janus_font_tests PRIVATE janus_runtime)
    target_compile_options(janus_font_tests PRIVATE ${JANUS_WARN_FLAGS})

    # No driver contract in play — janus_format.c only pulls janus_bound.c
    # (both leaf objects in the archive), so this needs no host mock, same
    # as janus_font_tests.
    add_executable(janus_format_tests tests/test_format.c)
    target_link_libraries(janus_format_tests PRIVATE janus_runtime)
    target_compile_options(janus_format_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_render_async_tests tests/test_render_async.c)
    target_link_libraries(janus_render_async_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_render_async_tests PRIVATE ${JANUS_WARN_FLAGS})

    # janus_clear_screen's full-panel path is #ifdef'd on macros a project
    # only gets from janus_render_config.gen.h when it declares
    # display.background. Recompile janus_runtime.c straight into this
    # target with those macros forced, rather than linking the library
    # (built without them). Mock sources are compiled in directly for the
    # same reason — janus_host_mock links the plain janus_runtime.
    add_executable(janus_clear_screen_bg_tests
        tests/test_clear_screen_bg.c
        src/janus_runtime.c src/janus_input_touch.c src/janus_input_focus.c
        src/janus_font.c src/janus_bound.c src/janus_format.c
        host_mock/mock_driver.c host_mock/mock_touch.c
        host_mock/mock_encoder.c host_mock/mock_buttons.c)
    target_include_directories(janus_clear_screen_bg_tests PRIVATE include host_mock)
    target_compile_definitions(janus_clear_screen_bg_tests PRIVATE
        JANUS_RENDER_NONBLOCKING
        JANUS_DISPLAY_BACKGROUND=0x4321
        JANUS_DISPLAY_PANEL_W=48 JANUS_DISPLAY_PANEL_H=32)
    target_compile_options(janus_clear_screen_bg_tests PRIVATE ${JANUS_WARN_FLAGS})

    # janus_draw's helpers live in janus_runtime.o, which pulls the driver
    # contract — so this links the mock, same as janus_runtime_tests.
    add_executable(janus_remote_tests tests/test_remote.c)
    target_link_libraries(janus_remote_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_remote_tests PRIVATE ${JANUS_WARN_FLAGS})

    add_executable(janus_draw_tests tests/test_draw.c)
    target_link_libraries(janus_draw_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_draw_tests PRIVATE ${JANUS_WARN_FLAGS})

    enable_testing()
    add_test(NAME janus_runtime_tests COMMAND janus_runtime_tests)
    add_test(NAME janus_input_touch_tests COMMAND janus_input_touch_tests)
    add_test(NAME janus_input_focus_tests COMMAND janus_input_focus_tests)
    add_test(NAME janus_input_encoder_tests COMMAND janus_input_encoder_tests)
    add_test(NAME janus_input_buttons_tests COMMAND janus_input_buttons_tests)
    add_test(NAME janus_nav_focus_tests COMMAND janus_nav_focus_tests)
    add_test(NAME janus_font_tests COMMAND janus_font_tests)
    add_test(NAME janus_format_tests COMMAND janus_format_tests)
    add_test(NAME janus_draw_tests COMMAND janus_draw_tests)
    add_test(NAME janus_remote_tests COMMAND janus_remote_tests)
    add_test(NAME janus_render_async_tests COMMAND janus_render_async_tests)
    add_test(NAME janus_clear_screen_bg_tests COMMAND janus_clear_screen_bg_tests)
endif()
