/* Janus owns its entry point: keep SDL from rewriting `main` to `SDL_main`
 * (Windows) and demanding SDL2main be linked. Must precede <SDL.h>. */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "janus_runtime.h"
#include "janus_desktop_driver.h"
#include "janus_input_encoder.h"
#include "janus_input_focus.h"
#include "janus_actions.gen.h"

/* Window size comes from app.yaml's `display: size:` (baked into
 * janus_display_config.gen.h); an app with no `display:` block gets a
 * default window instead. */
#if defined(__has_include)
#  if __has_include("janus_display_config.gen.h")
#    include "janus_display_config.gen.h"
#  endif
#endif
#ifndef JANUS_DISPLAY_WIDTH
#  define JANUS_DISPLAY_WIDTH 320
#  define JANUS_DISPLAY_HEIGHT 240
#endif

extern janus_app_t janus_app;

/* janus_encoder_poll() (janus_input_encoder.h) is the rotary-encoder source — the default
 * SDL implementation is scaffolded once into src/desktop_input.c
 * (desktop_scaffold epic task 2) and is yours to edit from then on. */

int main(void) {
    if (!janus_desktop_driver_init(JANUS_DISPLAY_WIDTH, JANUS_DISPLAY_HEIGHT, "Janus")) return 1;
    janus_render_screen(janus_app_get_screen(&janus_app, janus_app.active_screen));
    janus_render_status_bar(&janus_app);   /* app-level status band (no-op if app.yaml has no `status:`) */
    janus_render_nav_bar(&janus_app);   /* app-level tab strip (no-op if app.yaml has no `nav:`) */
    /* Tabs are reachable from a single control by folding the nav strip into focus
     * traversal (nav_tabs epic task 4) — janus_focus_move/activate take &janus_app, not just
     * the active screen, so they can land focus "on the nav bar" and commit a tab switch. A
     * project with a spare control can also call janus_nav_next(&janus_app) / janus_nav_prev(...). */
    janus_focus_move(&janus_app, 0);   /* establish initial focus */

    /* pump() drains and discards SDL events (it only detects the close
     * button) — input is polled from SDL *state* by the poll function
     * instead. SDL_Delay keeps the loop from spinning a full core. */
    while (janus_desktop_driver_pump()) {
        SDL_Delay(1);
        janus_encoder_event_t event;
        int16_t delta;
        if (!janus_encoder_poll(&event, &delta)) continue;

        if (event == JANUS_ENCODER_ROTATE) {
            janus_focus_move(&janus_app, delta);
            continue;
        }

        /* JANUS_ENCODER_CLICK */
        janus_input_result_t hit = janus_focus_activate(&janus_app);
        switch (hit.kind) {
            case JANUS_INPUT_ACTION:
                janus_handle_action((janus_action_t)hit.action);
                break;
            case JANUS_INPUT_NAVIGATE:
                janus_switch_screen(&janus_app, (uint16_t)hit.navigate_target);
                janus_focus_move(&janus_app, 0);
                break;
            case JANUS_INPUT_TOGGLE_BOX:
                janus_toggle_box(hit.widget);
                break;
            default:
                break;
        }
    }

    janus_desktop_driver_shutdown();
    return 0;
}
