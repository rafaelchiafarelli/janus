/* Remote UI state — lets a host *mirror* a device's UI (the desktop
 * "mirror" mode, desktop_windows_mirror epic mirror_mode). The device is
 * the single source of UI truth; a viewer applies what the device reports
 * and never lets its own input change the screen.
 *
 *   device:  janus_remote_state_get(&app, &state);    -> serialize `state`
 *   viewer:  deserialize into `state`;  janus_remote_state_apply(&app, &state);
 *
 * Only *UI* state is here. Bound values (what the widgets display) live in
 * the app's generated janus_bindings.gen.h struct, which the transport
 * already has to carry: the viewer writes that struct, then redraws with
 * janus_render_screen_if_dirty. Nothing in this header reads or writes it.
 *
 * Its own translation unit (janus_remote.c): an app that never calls it
 * links none of it.
 */
#ifndef JANUS_REMOTE_H
#define JANUS_REMOTE_H

#include <stdbool.h>
#include <stdint.h>

#include "janus_runtime.h"

typedef struct {
    uint16_t screen;          /* app->active_screen */
    int16_t  focus;           /* focused widget's index among the active screen's reachable
                               * focusable widgets, in janus_focus_move's traversal order;
                               * -1 = no widget focused */
    int16_t  nav_focus;       /* previewed nav-strip tab (index into app->nav_tabs); -1 = none.
                               * At most one of focus / nav_focus is >= 0. */
    uint16_t boxes_expanded;  /* bit i set = the i-th box of the active screen's widget tree
                               * (depth-first, tree order — counted whether or not its parent
                               * is expanded) is expanded. 16 boxes, JANUS_MAX_BOXES; bits for
                               * boxes past the 16th are always 0 and never applied. */
} janus_remote_state_t;

/* Device side: snapshot the current UI state. */
void janus_remote_state_get(const janus_app_t *app, janus_remote_state_t *out);

/* Viewer side: make `app`'s UI exactly `state` — sets the active screen's
 * box states, does a full erase + redraw of that screen (and the status /
 * nav bars), then puts focus where `state` says. A pure function of
 * `state` (plus the bound values), so the result is pixel-comparable to
 * the device's own display.
 *
 * Fires NO actions, navigation or box toggles — it is not an input path.
 * Returns true if it redrew; false, drawing nothing, if `state` already
 * matches the current UI (a host may apply on every telemetry tick without
 * flicker) or if it is out of range: `screen` >= screen_count, `focus` or
 * `nav_focus` < -1, or `nav_focus` naming a tab the app doesn't have. A
 * `focus` index past the last reachable focusable widget is not rejected —
 * the screen still redraws, with nothing focused. */
bool janus_remote_state_apply(janus_app_t *app, const janus_remote_state_t *state);

#endif /* JANUS_REMOTE_H */
