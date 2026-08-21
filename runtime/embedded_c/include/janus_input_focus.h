/* Janus embedded-C runtime — Stage 6, shared focus core. See architecture.md.
 *
 * Fixed library, shared by both encoder and button modalities
 * (janus_input_encoder.c / janus_input_buttons.c) — the "front end"
 * differs per modality (a rotation vs a discrete GPIO edge), but moving
 * focus and activating whatever's currently focused is identical either
 * way, so it lives here once instead of twice. Mirrors
 * janus_input_touch.c's own split: this module only decides *which*
 * widget, never *how* to draw it (janus_runtime.c's janus_set_focus does
 * that) or *how* to dispatch an action (the caller — normally the
 * scaffolded main.c's event loop — does that, same as touch).
 */
#ifndef JANUS_INPUT_FOCUS_H
#define JANUS_INPUT_FOCUS_H

#include "janus_runtime.h"

/* Moves focus by `delta` among this screen's focusable widgets
 * (JANUS_FOCUS_NONE-tagged widgets are skipped, as is a collapsed box's
 * children — same rule janus_input_touch.c's hit-test uses), wrapping
 * around at either end. `delta == 0` doesn't move anything — it's the
 * idiom for "(re-)establish focus on this screen" (e.g. right after the
 * first janus_render_screen, or right after janus_switch_screen), since
 * it still runs the "nothing focused yet" bootstrap below. No-op if the
 * screen has no focusable widgets at all. */
void janus_focus_move(const janus_screen_desc_t *screen, int16_t delta);

/* Resolves whatever's currently focused into the same result shape
 * janus_touch_hit_test produces, so callers dispatch it identically
 * (ACTION/NAVIGATE/TOGGLE_BOX). JANUS_INPUT_NONE if nothing is focused,
 * or if the focused widget isn't actually part of `screen` (stale focus
 * from a screen that was just switched away from). */
janus_input_result_t janus_focus_activate(const janus_screen_desc_t *screen);

#endif /* JANUS_INPUT_FOCUS_H */
