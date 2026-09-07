# Task 3: nav-bar-input (touch + the dedicated cycle helper)

## Contract

The tab strip becomes interactive for touch, and gets a public
tab-cycling helper for projects with a control to spare. **The
focus-stop half of epic decision 3 is split out to task 4** — it's an
invasive change to `janus_input_focus.c`'s core (a synthetic focusable
entity, `app` threaded through `janus_focus_move`/`_activate`, ~a dozen
`test_input_focus.c` fixtures), well past what belongs in one task file.
Decision 3 still stands in full; it just lands across tasks 3 + 4.

### In

- Task 1's `janus_nav_tab_t[]` (rect + `target`).
- Task 2's `draw_nav_bar` (so a switch repaints the strip).

### Delivered

- **`janus_nav_hit_test(const janus_app_t *app, int16_t x, int16_t y)`**
  (`janus_input_touch.c` / `.h`): point-in-rect over the baked cells; a
  hit → `{ kind: JANUS_INPUT_NAVIGATE, navigate_target: tab.target }`,
  `JANUS_INPUT_NONE` otherwise (incl. no nav). Reuses the file's
  `point_in_rect`; loads each cell pgm-safe (`janus_nav_tab_load`).
- **`janus_nav_next(janus_app_t *app)` / `janus_nav_prev(...)`**
  (`janus_runtime.c` / `.h`): find the tab whose `target ==
  active_screen`, step `±1` in nav order (wrapping), `janus_switch_screen`
  to that tab's target (which already erases + re-renders + repaints the
  strip). No-op without nav. Focus re-establishment left to the caller,
  same as `janus_switch_screen`.
- **Touch scaffolds** (`main_touch.c.tmpl`, `main_touch_async.c.tmpl`):
  `janus_nav_hit_test(&janus_app, x, y)` first; on `NONE`, fall through
  to `janus_touch_hit_test(screen, …)`. The existing `JANUS_INPUT_NAVIGATE`
  case handles the rest.
- **Encoder/button scaffolds**: a comment pointing at `janus_nav_next/
  prev` for a project with a spare control; no wiring (nothing to bind to
  on a single control until task 4's focus-stop).
- **Docs**: `architecture.md` Stage 6 gets a "Nav strip — hit-testing"
  paragraph; `Janus.md`'s `tabs` row updated.

### Not in scope

- The focus-stop (task 4).
- Per-widget `navigate:` behaviour (already works); swipe/gesture nav.

## Dependencies

- **Task 1 + Task 2** merged to `tasks`.

## Pre-work

None beyond the epic fixture.

## Tests

- `runtime/embedded_c/tests/test_input_touch.c`: a tap in cell 2's rect →
  `NAVIGATE` target 2; a tap below the `NAV_BAR_H` band → `NONE`; `NONE`
  for a no-nav / NULL app. `janus_nav_next`/`_prev` cycle + wrap
  `active_screen`.
- `tests/test_scaffold_main.py`: the touch scaffold emits
  `janus_nav_hit_test(&janus_app, x, y)` and `janus_render_nav_bar`, with
  the nav check ordered before `janus_touch_hit_test(screen`.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · docs updated · this file
`git mv`-ed to `3-nav-bar-input-done.md` · commit + merge
`3-nav-bar-input → tasks`.
