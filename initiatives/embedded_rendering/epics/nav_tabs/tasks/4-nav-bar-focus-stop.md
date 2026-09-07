# Task 4: nav-bar-focus-stop

## Contract

Make the tab strip reachable from a **single**-control encoder/button
scaffold by folding it into the focus traversal: past the last (or before
the first) focusable widget, focus lands "on the nav bar"; a rotation /
NEXT-PREV there cycles the active tab, and it commits immediately (or on
click/SELECT). This is the second half of epic decision 3 — split from
task 3 because it reworks `janus_input_focus.c`'s core rather than adding
alongside it.

### In

- Task 1 `janus_nav_tab_t[]`, task 2 `draw_nav_bar`, task 3
  `janus_nav_next/prev`.

### Delivered (shape — settle the specifics when picking this up)

- **`janus_input_focus.c`**: a synthetic "nav focus" position that
  `janus_focus_move` can land on. Options to weigh: (a) index `count` in
  the existing wrap arithmetic; (b) a separate `g_on_nav` flag alongside
  `g_focused_widget`. Either way `janus_focus_move`/`janus_focus_activate`
  need `app` (to read `nav_tab_count` and call `janus_nav_next/prev` /
  `janus_switch_screen`) — a signature change rippling into
  `janus_input_encoder.h`/`buttons.h` usage and all four
  `main_*.c.tmpl` scaffolds.
- **`janus_runtime.c`**: `janus_set_focus` (or a sibling) able to draw
  the focus ring on the *active nav cell* rect, not a widget — the ring
  currently only knows `janus_widget_desc_t`.
- **Scaffolds**: encoder/buttons pass `&janus_app` into the focus calls;
  the `JANUS_INPUT_NAVIGATE`-from-nav path re-establishes focus on the
  new screen.
- **Docs**: `architecture.md` Stage 6 focus section; `Janus.md`.

### Not in scope

- Touch (task 3, done).
- Changing the fixed constants / styling (decision 2).

## Dependencies

- **Tasks 1–3** merged to `tasks`.
- A decision on the focus-position representation (a) vs (b) above, and
  on whether a nav-focused rotation commits immediately or only on
  click/SELECT — bring both to Rafael before starting.

## Pre-work

None beyond the epic fixture.

## Tests

- `runtime/embedded_c/tests/test_input_focus.c` (or a new
  `test_nav_focus.c`): rotating past the last widget lands on nav focus;
  a further rotation there advances `active_screen` and wraps; the ring
  is drawn on the active cell; leaving nav focus (rotate back) returns to
  a real widget.
- `tests/test_scaffold_main.py`: encoder/buttons scaffolds thread
  `&janus_app` into the focus calls.

## DoD

Contract delivered · full Python + `ctest` suites green ·
`scripts/avr_gate.sh` green · docs updated · this file `git mv`-ed to
`4-nav-bar-focus-stop-done.md` · commit + merge `4-nav-bar-focus-stop →
tasks` · then the epic acceptance gate runs before `tasks → nav_tabs`.
