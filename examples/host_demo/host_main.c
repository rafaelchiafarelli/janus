/* Host-only harness for this example — src/main.c (Stage 8's real
 * scaffold) has the correct shape for real firmware: init, render once,
 * loop forever polling for touch. That loop can't run to completion as
 * a verification step, so this drives the same poll -> hit-test ->
 * dispatch logic (Stage 6) against a couple of simulated taps, then
 * exits. Compiling src/main.c itself (the `host_demo_firmware_main`
 * CMake target) is what proves the real scaffold links against the
 * runtime — this target is what proves it actually renders *and reacts*.
 */
#include <stdio.h>

#include "janus_actions.gen.h"
#include "janus_input_touch.h"
#include "janus_runtime.h"
#include "mock_driver.h"
#include "mock_touch.h"

extern janus_app_t janus_app;
void display_driver_init(void);

static void render_and_summarize(const char *label) {
    mock_driver_reset();
    janus_render_screen(janus_app.screens[janus_app.active_screen]);

    printf("%s: %u draw_area_sync call(s)\n", label, (unsigned)mock_driver_log_count);
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        printf(
            "  [%2u] x=%3u y=%3u w=%3u h=%3u fill=0x%02x\n", i,
            mock_driver_log[i].x, mock_driver_log[i].y,
            mock_driver_log[i].w, mock_driver_log[i].h,
            mock_driver_log[i].sample_byte
        );
    }
}

/* Same poll -> hit-test -> 3-way dispatch shape as the scaffolded
 * src/main.c's event loop — inlined here so the demo can narrate each
 * step and exit afterward instead of looping forever. */
static void simulate_tap(int16_t x, int16_t y) {
    mock_touch_queue_tap(x, y);
    int16_t tx, ty;
    if (!janus_touch_poll(&tx, &ty)) return;

    const janus_screen_desc_t *screen = janus_app.screens[janus_app.active_screen];
    janus_input_result_t hit = janus_touch_hit_test(screen, tx, ty);
    switch (hit.kind) {
        case JANUS_INPUT_ACTION:
            printf("tap (%d,%d): ACTION -> janus_handle_action\n", tx, ty);
            janus_handle_action((janus_action_t)hit.action);
            break;
        case JANUS_INPUT_NAVIGATE:
            printf("tap (%d,%d): NAVIGATE -> screen %d\n", tx, ty, hit.navigate_target);
            janus_switch_screen(&janus_app, (uint16_t)hit.navigate_target);
            break;
        case JANUS_INPUT_TOGGLE_BOX:
            printf("tap (%d,%d): TOGGLE_BOX -> %s\n", tx, ty, hit.widget->id);
            janus_toggle_box(hit.widget);
            break;
        default:
            printf("tap (%d,%d): NONE\n", tx, ty);
            break;
    }
}

int main(void) {
    display_driver_init();

    render_and_summarize("initial render (diagnostics box starts collapsed)");

    printf("\n");
    simulate_tap(10, 80);   /* inside diagnostics_box's header: {0,72,24,16} */
    render_and_summarize("re-render after the tap (box now expanded)");

    printf("\n");
    simulate_tap(10, 140);  /* inside reboot_button: {0,132,64,20} */

    return 0;
}
