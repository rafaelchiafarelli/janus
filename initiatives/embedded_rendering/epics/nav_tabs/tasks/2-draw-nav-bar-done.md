# Task 2: draw-nav-bar

## Contract

The fixed runtime draws the tab strip from task 1's descriptor: N
equal-width cells, each title centered, the cell whose index ==
`app->active_screen` styled distinct. Painted on screen entry and on
`janus_switch_screen`, **never on the periodic render tick**.

### In

- `janus_app_t.nav_tabs` / `.nav_tab_count` + `janus_nav_tab_t` (task 1).
- `app->active_screen`.

### Delivered

- **`runtime/embedded_c/src/janus_runtime.c`**:
  - `static void draw_nav_bar(const janus_app_t *app)` — for each tab:
    `fill_rect(tab.rect, inactive_bg)` then
    `draw_string(tab.rect, title, ink, bg, /*from_flash*/true, font,
    scale, /*center*/true, /*allow_shrink*/false)` (buttons-style: no
    shrink, so all cells match — reuses the round-2 `draw_string`
    signature). The `i == app->active_screen` cell instead gets
    `active_bg` + a `JANUS_FOCUS_RING_W`-ish bottom accent bar.
  - Colours: fixed runtime constants (`JANUS_COLOR_NAV_*`) unless open
    decision 2 makes them authorable.
  - Public entry `void janus_render_nav_bar(const janus_app_t *app)` in
    `janus_runtime.h`.
  - `janus_switch_screen` / `janus_switch_screen_async_start` call
    `draw_nav_bar(app)` after `janus_render_screen` for the new screen
    (they already have `app`). The strip sits in the `NAV_BAR_H` band
    task 1 reserved, so it never overpaints screen content.
  - `janus_clear_screen`: the full-panel path already covers the band;
    the best-effort path must include the band in its union (or the
    caller redraws the strip right after — which `janus_switch_screen`
    does anyway).
- **Scaffold** (`janus/templates/main_*.c.tmpl`): call
  `janus_render_nav_bar(&janus_app)` once after the initial
  `janus_render_screen`, and nowhere in the periodic path. (Input wiring
  is task 3 — this task just gets it on screen.)
- **Docs**: `architecture.md` Stage 4 (new `draw_nav_bar`), `Janus.md`
  (tab strip is now real).

### Not in scope

- Touch / encoder / button dispatch into the strip (task 3).
- Authorable styling beyond whatever open decision 2 lands.

## Dependencies

- **Task 1** (descriptor + geometry + panel size reaching the runtime).
- Decision 2 settled: `NAV_BAR_H` + `JANUS_COLOR_NAV_*` are fixed
  constants; active cell = accent fill + ~6px bottom bar, inactive =
  outline.

## Pre-work

None beyond task 1's fixture.

## Tests (`runtime/embedded_c/tests/test_runtime.c`, new fixture)

- Hand-built `janus_app_t` with 3 nav tabs; `janus_render_nav_bar(&app)`
  →: exactly 3 title-bearing cells, spanning `[0, panel_w)` with no gap
  or overlap; the `active_screen` cell paints `active_bg` and the accent
  bar, the others don't.
- `janus_switch_screen` from screen 0 to 1 → the strip is redrawn with
  the highlight moved to cell 1 (scan the mock log by rect/colour).
- A bare `janus_render_screen(screen)` with no `app` in play draws **no**
  nav pixels (the strip is app-scoped, not screen-scoped) — i.e. the
  periodic tick that calls `janus_render_*_if_dirty` never touches it.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green (watch `.bss`/`.text` —
`janus_nav_tabs[]` is flash, `draw_nav_bar` is code) · docs updated ·
this file `git mv`-ed to `2-draw-nav-bar-done.md` · commit + merge
`2-draw-nav-bar → tasks`.
