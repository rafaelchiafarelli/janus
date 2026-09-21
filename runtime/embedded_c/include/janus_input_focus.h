/* Janus embedded-C runtime — Stage 6, shared focus core. See architecture.md.
 *
 * Fixed library, shared by both encoder and button modalities
 * (janus_input_encoder.c / janus_input_buttons.c) — the "front end"
 * differs per modality (a rotation vs a discrete GPIO edge), but moving
 * focus and activating whatever's currently focused is identical either
 * way, so it lives here once instead of twice. Mirrors
 * janus_input_touch.c's own split: this module only decides *which*
 * widget, never *how* to draw it (janus_runtime.c's janus_set_focus /
 * janus_set_nav_focus do that) or *how* to dispatch an action (the
 * caller — normally the scaffolded main.c's event loop — does that, same
 * as touch).
 *
 * Both entry points take the whole `janus_app_t` (not just the active
 * screen) as of nav_tabs epic task 4: an app with `nav: { kind: tabs }`
 * folds the nav strip into the same traversal as its screens' widgets —
 * moving past the last (or before the first) focusable widget lands
 * focus "on the nav bar" as a whole, and janus_focus_move from there
 * cycles a *previewed* tab (janus_get_nav_focus) instead of a widget,
 * independent of the per-screen widget count. Moving off either end of
 * that tab run hands focus back to a real widget (the last one going
 * backward off the bottom, the first one going forward off the top) —
 * the mirror image of how the nav strip was entered. An app with no nav
 * (`app->nav_tabs == NULL`) never enters this state; every call below
 * degrades to the plain per-screen wrap it always did.
 */
#ifndef JANUS_INPUT_FOCUS_H
#define JANUS_INPUT_FOCUS_H

#include "janus_runtime.h"

/* Moves focus by `delta` among `app`'s active screen's focusable widgets
 * (JANUS_FOCUS_NONE-tagged widgets are skipped, as is a collapsed box's
 * children — same rule janus_input_touch.c's hit-test uses) plus, when
 * the app has a nav strip, the strip itself as one extra stop past the
 * last widget. `delta == 0` doesn't move anything — it's the idiom for
 * "(re-)establish focus on this screen" (e.g. right after the first
 * janus_render_screen, or right after janus_switch_screen), since it
 * still runs the "nothing focused yet" bootstrap below. No-op if the
 * active screen has no focusable widgets and the app has no nav either. */
void janus_focus_move(janus_app_t *app, int16_t delta);

/* Resolves whatever's currently focused into the same result shape
 * janus_touch_hit_test produces, so callers dispatch it identically
 * (ACTION/NAVIGATE/TOGGLE_BOX). JANUS_INPUT_NONE if nothing is focused,
 * or if the focused widget isn't actually part of `app`'s active screen
 * (stale focus from a screen that was just switched away from).
 *
 * If the nav strip is focused instead, this commits the previewed tab:
 * janus_switch_screen(app, ...) to its target, then janus_focus_move(app,
 * 0) to re-establish focus on the new screen — both already done by the
 * time this returns, so it hands back JANUS_INPUT_NONE (nothing left for
 * the caller to dispatch), unlike touch's janus_nav_hit_test which only
 * resolves the target and leaves switching to the caller. */
janus_input_result_t janus_focus_activate(janus_app_t *app);

/* The focused widget's position among `app`'s active screen's reachable
 * focusable widgets, in janus_focus_move's traversal order; -1 if no
 * widget is focused (including when the nav strip is). Together with
 * janus_focus_set_index this is the stable, device-independent name for
 * "what has focus" that janus_remote.h serializes. */
int16_t janus_focus_index(const janus_app_t *app);

/* Focuses the widget at that position (redrawing rings like
 * janus_set_focus); a negative `index` clears widget focus. Returns false,
 * changing nothing, if `index` is past the last reachable focusable widget. */
bool janus_focus_set_index(janus_app_t *app, int16_t index);

#endif /* JANUS_INPUT_FOCUS_H */
