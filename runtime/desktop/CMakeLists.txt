cmake_minimum_required(VERSION 3.16)
project(janus_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

# Same target name the embedded_c target's CMakeLists.txt defines — a
# consumer's own target_link_libraries(x PRIVATE janus_runtime) doesn't
# need to know or care which target generated this runtime/ tree.
#
# Sources are exactly the same platform-agnostic core embedded_c's own
# CMakeLists.txt compiles (janus_runtime.c and friends land in this same
# vendored src/ via the shared embedded_c vendor_from root — see
# janus/targets.py). No AVR linker-script handling here — genuinely N/A
# off-AVR. Deliberately does NOT include the SDL2 driver (below) — this
# library still declares draw_area_sync/draw_area_async/display_busy as
# extern, same as the embedded_c build, so it links against *either* a
# real driver or the mock, exactly like embedded_c does.
add_library(janus_runtime STATIC
    src/janus_runtime.c
    src/janus_input_touch.c
    src/janus_input_focus.c
    src/janus_font.c
    src/janus_bound.c
    src/janus_format.c
)
target_include_directories(janus_runtime PUBLIC include)
target_compile_options(janus_runtime PRIVATE -Wall -Wextra)

# The real SDL2 driver — a separate library, deliberately never linked
# together with janus_host_mock below (both define draw_area_sync/
# draw_area_async/display_busy; linking both is a multiple-definition
# error, confirmed while building this). A consumer's own executable
# (examples/desktop_demo, or any project's real app) links
# janus_runtime + janus_desktop_driver; the mock-based tests below link
# janus_runtime + janus_host_mock instead. Same relationship
# display_driver_stub.c has to embedded_c's own janus_runtime — a real
# driver and the test mock are always alternatives, never both at once.
add_library(janus_desktop_driver STATIC driver/janus_desktop_driver.c)
target_link_libraries(janus_desktop_driver PUBLIC janus_runtime SDL2::SDL2)
target_compile_options(janus_desktop_driver PRIVATE -Wall -Wextra)

option(JANUS_RUNTIME_BUILD_TESTS "Build janus_runtime's own host-side tests" ON)
if(JANUS_RUNTIME_BUILD_TESTS)
    # Same reasoning as embedded_c's own CMakeLists.txt: the host suite
    # exercises the polled/async render path, which janus_runtime.c
    # compiles only under JANUS_RENDER_NONBLOCKING.
    target_compile_definitions(janus_runtime PUBLIC JANUS_RENDER_NONBLOCKING)

    add_library(janus_host_mock STATIC
        host_mock/mock_driver.c
        host_mock/mock_touch.c
        host_mock/mock_encoder.c
        host_mock/mock_buttons.c
    )
    target_include_directories(janus_host_mock PUBLIC host_mock)
    target_link_libraries(janus_host_mock PUBLIC janus_runtime)

    # Mock-based tests: identical to embedded_c's own — they exercise
    # janus_runtime.c against the scripted mock driver, nothing SDL2-
    # specific, and still make sense to run for this target too.
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

    add_executable(janus_nav_focus_tests tests/test_nav_focus.c)
    target_link_libraries(janus_nav_focus_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_nav_focus_tests PRIVATE -Wall -Wextra)

    add_executable(janus_font_tests tests/test_font.c)
    target_link_libraries(janus_font_tests PRIVATE janus_runtime)
    target_compile_options(janus_font_tests PRIVATE -Wall -Wextra)

    add_executable(janus_format_tests tests/test_format.c)
    target_link_libraries(janus_format_tests PRIVATE janus_runtime)
    target_compile_options(janus_format_tests PRIVATE -Wall -Wextra)

    add_executable(janus_render_async_tests tests/test_render_async.c)
    target_link_libraries(janus_render_async_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_render_async_tests PRIVATE -Wall -Wextra)

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
    target_compile_options(janus_clear_screen_bg_tests PRIVATE -Wall -Wextra)

    add_executable(janus_draw_tests tests/test_draw.c)
    target_link_libraries(janus_draw_tests PRIVATE janus_runtime janus_host_mock)
    target_compile_options(janus_draw_tests PRIVATE -Wall -Wextra)

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
    add_test(NAME janus_render_async_tests COMMAND janus_render_async_tests)
    add_test(NAME janus_clear_screen_bg_tests COMMAND janus_clear_screen_bg_tests)
endif()
