# Task 1: nav-bar-geometry-and-descriptor

## Contract

When `app.nav` is set, Stage 2 reserves a fixed `NAV_BAR_H` band at the
top of every screen and Stage 3b bakes a per-app **nav descriptor** the
runtime can draw from. **No rendering in this task** — data + geometry
only, asserted through generated output and layout unit tests.

### In

- `app.yaml` `nav: { kind: tabs, targets: [{screen, title}, ...] }` —
  already parsed to `App.nav: list[NavTarget]`.
- `app.display.size` — see Dependencies / open decision 1.

### Delivered

- **`janus/stage2_layout/layout.py`**: a module constant `NAV_BAR_H`
  (value per open decision 2). `layout_screen` offsets the screen root's
  origin to `y = NAV_BAR_H` (and shrinks the height available to a
  top-level `fill:` child by `NAV_BAR_H`) **iff** a nav is present — a
  new `has_nav: bool` param, defaulting `False` so every existing call
  and every no-nav screen lays out byte-identical. `check_fits_display`
  accounts for the band.
- **Per-tab geometry**: N tabs laid out as equal-width cells across
  `display.width` (last cell absorbs the rounding remainder, same rule as
  `_distribute_fill`), each `NAV_BAR_H` tall at `y = 0`.
- **`janus/ir.py`**: a `NavBar`/`NavTab` shape (or extend the existing
  `NavTarget`) carrying, per tab: `rect`, `title`, `target_screen_index`.
  Attached to `App` (e.g. `App.nav_bar`), populated by a small
  Stage-2/Stage-3b helper — not authored.
- **`janus/stage3b_embedded_c/emit_embedded_c.py`** (`emit_app_table`):
  emit the descriptor next to `janus_app_screens[]` — a
  `static const janus_nav_tab_t janus_nav_tabs[] JANUS_PROGMEM` (rect +
  flash title ptr + `int16_t target`) plus `janus_app_t.nav_tabs` /
  `.nav_tab_count`. Keep `nav_titles` or fold it in — pick one, document
  which. `emit_render_config` also emits `JANUS_DISPLAY_PANEL_W/H`
  whenever `app.nav` is set (today only for `display.background`).
- **`runtime/embedded_c/include/janus_runtime.h`**: the `janus_nav_tab_t`
  struct + the two new `janus_app_t` fields. No behaviour.
- **Validation** (`janus/stage1_parse/dsl_yaml.py` or Stage 2): `nav` set
  but `display.size` unset → `ValueError` (decision 1); tabs whose total
  min width can't fit `display.width` → `ValueError` (no scrolling, see
  epic out-of-scope).
- **Docs**: `architecture.md` Stage 2 (the band) + Stage 3b app-table
  section (the descriptor); `Janus.md` `nav` note updated from "metadata
  only, nothing renders it".

### Not in scope

- `draw_nav_bar` / any pixels (task 2).
- Any input path (task 3).
- Touching `nav.kind` (stays hard-coded `tabs`).

## Dependencies

- Epic decisions 1/2/4 are settled (see `epic.md`): `display.size`
  mandatory with `nav`; `NAV_BAR_H` a fixed layout constant; strip is a
  separate band at `y = 0` with the screen (its own header row intact)
  starting at `y = NAV_BAR_H`.
- Builds on `display.background`'s `janus_render_config.gen.h` panel-size
  seam (`JANUS_DISPLAY_PANEL_W/H`, `fixes/000003`, on `dev`) — that's the
  route for getting width to the runtime; extend it to emit those macros
  whenever `nav` is set too, not only when `background` is.

## Pre-work

- A `tests/fixtures/app_with_nav.yaml` (+ its screen files) exercising 3
  tabs on a known panel size. Plain YAML, no code — fine to create as
  planning pre-work.

## Tests

- `tests/test_layout.py`: nav present → root at `y = NAV_BAR_H`; a
  top-level `fill:` child's height reduced by `NAV_BAR_H`; no-nav screen
  unchanged; `check_fits_display` rejects a screen that + band exceeds
  the panel.
- `tests/test_dsl_yaml_app.py`: `nav` without `display.size` raises; tabs
  that can't fit the width raise.
- `tests/test_emit_embedded_c_app.py` / `test_emit_files.py`: the
  `janus_nav_tabs[]` array + `janus_app_t.nav_tab_count` appear with the
  right rects/titles/targets; absent when `nav` is unset.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green (no runtime behaviour changed — struct only) ·
`scripts/avr_gate.sh` green · docs updated · this file `git mv`-ed to
`1-nav-bar-geometry-and-descriptor-done.md` · commit + merge
`1-nav-bar-geometry-and-descriptor → tasks`.
