cmake_minimum_required(VERSION 3.16)
project(janus_host_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/embedded_c)
set(JANUS_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${RUNTIME_DIR} janus_runtime_build)

set(GENERATED_SOURCES
    build/generated/devicestatus_screen.gen.c
    build/generated/janus_app.gen.c
)
set(COMMON_SOURCES
    ${GENERATED_SOURCES}
    janus_bindings.c
    src/janus_actions.c
    display_driver_stub.c
    ${RUNTIME_DIR}/host_mock/mock_driver.c
)
set(COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/build/generated
    ${RUNTIME_DIR}/host_mock
)

# Renders once and prints what the mock driver recorded — this is what
# actually gets run by the verification step.
add_executable(host_demo host_main.c ${COMMON_SOURCES})
target_include_directories(host_demo PRIVATE ${COMMON_INCLUDES})
target_link_libraries(host_demo PRIVATE janus_runtime)
target_compile_options(host_demo PRIVATE -Wall -Wextra)

# Links Stage 8's real, unmodified src/main.c against everything —
# proves the actual scaffolded firmware entry point compiles and links.
# Not run: its while(1) event loop is correct for real firmware but has
# no exit for a host verification step.
add_executable(host_demo_firmware_main src/main.c ${COMMON_SOURCES})
target_include_directories(host_demo_firmware_main PRIVATE ${COMMON_INCLUDES})
target_link_libraries(host_demo_firmware_main PRIVATE janus_runtime)
target_compile_options(host_demo_firmware_main PRIVATE -Wall -Wextra)
