# Task 3: nav-bar-input

## Contract

Make the tab strip interactive: a touch/tap in a tab cell navigates to
that screen, and the encoder/button modality gets a defined
tab-switching interaction (per epic open decision 3). Update the
scaffolds and the docs that currently say `nav` is "metadata only, not a
wired input path".

### In

- Task 1's `janus_nav_tab_t[]` (rect + `target_screen_index`).
- Task 2's `draw_nav_bar` (so a switch repaints the strip).

### Delivered

- **Touch** (`runtime/embedded_c/src/janus_input_touch.c`): before the
  per-screen widget hit-test, test the point against the nav band; a hit
  in tab `i` returns `{ kind: JANUS_INPUT_NAVIGATE, navigate_target: i }`.
  Needs the `janus_app_t` (or the nav descriptor) threaded into the
  touch entry point — mirror how `janus_render_nav_bar` gets `app`.
- **Encoder / buttons — both paths (decision 3, settled):**
  - dedicated `janus_nav_next(app)` / `janus_nav_prev(app)` in a small
    nav-input helper, bound by the scaffold to a specific control
    (mirrors ArduinoIHM's rot0-drives-tabs); **and**
  - the strip is a focus stop: `janus_input_focus.c` treats the nav bar
    as a focusable pseudo-widget; rotation while it's focused cycles
    `active_screen`, click commits.
  Both end in `janus_switch_screen(app, target)` +
  `janus_render_nav_bar(app)`.
- **Scaffolds** (`janus/templates/main_touch*.c.tmpl`,
  `main_encoder*.c.tmpl`, `main_buttons*.c.tmpl`): wire the above. The
  touch scaffold routes a `JANUS_INPUT_NAVIGATE` from the strip through
  the existing `janus_switch_screen` path; the encoder/buttons scaffolds
  gain the decision-3 binding.
- **Docs**: `architecture.md` Stage 6 — replace "`navigate` values ...
  never actually dispatched" / "`nav` is metadata only" with the real
  path; `Janus.md` input section.

### Not in scope

- Changing per-widget `navigate:` button behaviour (already works).
- Gesture/swipe navigation.

## Dependencies

- **Task 1 + Task 2** both merged to `tasks`.
- Decision 3 settled: implement *both* the dedicated `janus_nav_next/prev`
  helper and the focus-stop behaviour.

## Pre-work

None beyond the epic fixture.

## Tests

- `runtime/embedded_c/tests/test_input_touch.c`: a tap at a coordinate
  inside tab cell 2's rect → `JANUS_INPUT_NAVIGATE` with
  `navigate_target == 2`; a tap below the band still hits the screen's
  widgets as before.
- `runtime/embedded_c/tests/test_input_focus.c` (or a new
  `test_nav_input.c` for model (b)): the chosen encoder/button gesture
  advances/retreats `active_screen` and wraps.
- `tests/test_scaffold_main.py`: each scaffold variant emits the nav
  dispatch call.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · docs updated · this file
`git mv`-ed to `3-nav-bar-input-done.md` · commit + merge
`3-nav-bar-input → tasks` · then the epic acceptance gate (epic.md) is
run before `tasks → nav_tabs`.
