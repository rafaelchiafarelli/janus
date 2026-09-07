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

### Decisions (settled with Rafael 2026-09-07)

- **Focus-position representation: index `count`** in the existing wrap
  arithmetic — the nav strip is position N among N focusable widgets, so
  `janus_focus_move`'s modulo already reaches it. `widget_at()` /
  `focus_position()` / `janus_set_focus` special-case the out-of-range
  index; no new `g_on_nav` state.
- **Commit timing: preview, commit on click/SELECT.** Rotating (or
  NEXT/PREV) while nav-focused moves a highlight across the tab cells
  *without* switching screens; `janus_focus_activate` on the highlighted
  cell is what calls `janus_switch_screen`. Matches widget focus
  (move-then-activate). So there's a "previewed tab index" to track while
  nav-focused, distinct from `active_screen`.

### Delivered (shape)

- **`janus_input_focus.c`**: nav strip as focus index `count`. A
  `previewed_tab` cursor (starts at the tab showing `active_screen`)
  advances/wraps on `janus_focus_move` while nav-focused. `janus_focus_
  move`/`janus_focus_activate` take `app` (for `nav_tab_count`, and to
  `janus_switch_screen` on activate) — signature change rippling into the
  encoder/buttons scaffolds.
- **`janus_runtime.c`**: `janus_set_focus` (or a sibling) can draw the
  focus ring on the *previewed nav cell* rect, not a widget — the ring
  currently only knows `janus_widget_desc_t`. On activate,
  `janus_switch_screen(app, previewed_tab.target)` then
  `janus_focus_move(new_screen, 0)`.
- **Scaffolds** (all four encoder/buttons templates): pass `&janus_app`
  into the focus calls; on a NAVIGATE result from nav-focus, re-establish
  focus on the new screen.
- **Docs**: `architecture.md` Stage 6 focus section; `Janus.md`.

### Not in scope

- Touch (task 3, done).
- Changing the fixed constants / styling (decision 2).

## Dependencies

- **Tasks 1–3** merged to `tasks` (and fast-tracked to `dev` 2026-09-07 —
  branch off `dev` for this task).
- Both design decisions are settled (see Decisions above).

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
