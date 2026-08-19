#include "janus_runtime.h"

extern janus_app_t janus_app;

void display_driver_init(void);  /* vendor-provided, see src/display_driver.c */

int main(void) {
    display_driver_init();
    janus_render_screen(janus_app.screens[janus_app.active_screen]);

    while (1) {
        /* TODO: event loop — input dispatch (Stage 6) lands here */
    }

    return 0;
}
