# Task 2: draw-and-wire-status-bar

## Contract

`draw_status_bar(app)` in the fixed runtime paints the status band task
1 reserved — full-width fill + centered text — and is wired into every
call site `draw_nav_bar` already uses. Unlike `nav_tabs`, there's no
Stage 6 input path: the band is display-only, so this task also closes
out the epic (no task 3).

### In

- Task 1's baked `janus_app_t.status_bar` (`janus_status_bar_t*`, `NULL`
  when `app.yaml` has no `status:`).

### Delivered

- **`runtime/embedded_c/src/janus_runtime.c`**: `JANUS_COLOR_STATUS_BG`
  (`0x18e3`) / `JANUS_COLOR_STATUS_INK` (`0xffff`) `#define`s next to
  `JANUS_COLOR_NAV_*` (line ~1079). `static void draw_status_bar(const
  janus_app_t *app)` next to `draw_nav_bar` (line ~1086) — no-op if
  `app == NULL || app->status_bar == NULL` (mirrors `draw_nav_bar`'s own
  guard); `fill_rect` the band, `draw_string` the flash-loaded text
  centered, `JANUS_FONT_SIZE_MEDIUM`, no auto-shrink (same call shape
  `draw_nav_bar` uses for tab titles). A public `void
  janus_render_status_bar(const janus_app_t *app)` wrapper, mirroring
  `janus_render_nav_bar` (line 1114).
- **Call sites** — same three `draw_nav_bar` already has:
  `janus_switch_screen` (line ~1133, right after the existing
  `draw_nav_bar(app);` call), `janus_switch_screen_async_start` (line
  ~1211), and the redraw at line ~1249. Order: status bar above nav bar
  visually, but call order doesn't matter since they paint disjoint
  bands — keep them adjacent in source for readability.
- **All 6 scaffold templates** (`janus/templates/main_{touch,encoder,buttons}{,_async}.c.tmpl`):
  one `janus_render_status_bar(&janus_app);` next to the existing
  `janus_render_nav_bar(&janus_app);` line (e.g. `main_touch.c.tmpl:14`),
  same "no-op if app.yaml has no status:" comment style.
- **Never called from the periodic render tick** (`janus_render_*_if_dirty`
  sweep) — same rule `nav_tabs` established, and the reason ArduinoIHM's
  own status-bar flicker bug (`janus_handoff`, item 3) was a
  *consumer*-side mistake this task must not make easy to repeat.
- **Docs**: `architecture.md` Stage 4 (draw_status_bar, alongside the nav
  strip rendering paragraph) and Stage 8 (scaffold call sites); `Janus.md`
  updated from "status: metadata only" (whatever task 1 leaves it at) to
  reflect actual rendering.
- **Follow-up flag, not part of this task**: once this lands, ArduinoIHM's
  three hand-authored `status_bar` rows in its `.screen.yaml` files
  should be deleted in favor of the new app-level field — that's a note
  for the ArduinoIHM session, done there, not here.

### Not in scope

- Bound/live status text (epic out-of-scope, v1 is static text only).

## Dependencies

Task 1 delivered and merged (baked `status_bar` descriptor + `STATUS_BAR_H`
layout available).

## Tests

- `ctest`: a new `janus_runtime_tests` (or sibling) case asserting
  `draw_status_bar` paints the band at the right rect/colour on
  `janus_switch_screen`, and is a no-op when `status_bar == NULL` —
  mirrors the existing nav-bar draw tests.
- Manual/host: `examples/host_demo` with a `status:` field added shows
  the band as the true top-most row.

## DoD

Contract delivered · epic acceptance gate (see `epic.md`) passes ·
`python -m unittest discover -s tests` green · `ctest` green ·
`scripts/avr_gate.sh` green · docs updated · this file `git mv`-ed to
`2-draw-and-wire-status-bar-done.md` · commit + merge
`2-draw-and-wire-status-bar → tasks` · `tasks` merges up through
`status_bar → epics → app_chrome → features → dev` (epic's acceptance
gate is the last check before that final merge).
