/* Host-only harness for this example — src/main.c (Stage 8's real
 * scaffold) has the correct shape for real firmware: init, render once,
 * loop forever waiting for input. That can't be executed to completion
 * as a verification step, so this does the same boot sequence, renders
 * once, and prints what the mock driver recorded, then exits. Compiling
 * src/main.c itself (the `host_demo_firmware_main` CMake target) is what
 * proves the real scaffold links against the runtime — this target is
 * what proves it actually *renders*.
 */
#include <stdio.h>

#include "janus_runtime.h"
#include "mock_driver.h"

extern janus_app_t janus_app;
void display_driver_init(void);

int main(void) {
    display_driver_init();
    mock_driver_reset();
    janus_render_screen(janus_app.screens[janus_app.active_screen]);

    printf(
        "host_demo: rendered screen %u (%s), %u draw_area_sync call(s) recorded\n",
        janus_app.active_screen,
        janus_app.screens[janus_app.active_screen]->name,
        (unsigned)mock_driver_log_count
    );
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        printf(
            "  [%2u] x=%3u y=%3u w=%3u h=%3u fill=0x%02x\n", i,
            mock_driver_log[i].x, mock_driver_log[i].y,
            mock_driver_log[i].w, mock_driver_log[i].h,
            mock_driver_log[i].sample_byte
        );
    }
    return 0;
}
