#include "janus_runtime.h"
#include "janus_input_touch.h"
#include "janus_actions.gen.h"

extern janus_app_t janus_app;

void display_driver_init(void);  /* vendor-provided, see src/display_driver.c */
/* janus_touch_poll() (janus_input_touch.h) is the vendor-provided touch
 * source — implement it alongside display_driver_init(). */

int main(void) {
    display_driver_init();
    janus_render_screen(janus_app.screens[janus_app.active_screen]);

    while (1) {
        int16_t x, y;
        if (!janus_touch_poll(&x, &y)) continue;

        const janus_screen_desc_t *screen = janus_app.screens[janus_app.active_screen];
        janus_input_result_t hit = janus_touch_hit_test(screen, x, y);
        switch (hit.kind) {
            case JANUS_INPUT_ACTION:
                janus_handle_action((janus_action_t)hit.action);
                break;
            case JANUS_INPUT_NAVIGATE:
                janus_switch_screen(&janus_app, (uint16_t)hit.navigate_target);
                break;
            case JANUS_INPUT_TOGGLE_BOX:
                janus_toggle_box(hit.widget);
                break;
            default:
                break;
        }
    }

    return 0;
}
