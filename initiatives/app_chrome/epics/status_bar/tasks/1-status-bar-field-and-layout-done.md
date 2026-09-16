# Task 1: status-bar-field-and-layout

## Contract

When `app.yaml` declares `status: { text: "..." }`, Stage 2 reserves a
fixed `STATUS_BAR_H` band as the true top of every screen (above the nav
band, when `app.nav` is also set), and Stage 3b bakes a status
descriptor the runtime can draw from. **No rendering in this task** —
data + geometry only, mirroring `nav_tabs` task 1's split.

### In

- `app.yaml` `status: { text: "..." }` — new.
- `app.display.size` — required whenever `status` is set (epic decision 2).

### Delivered

- **`janus/ir.py`**: a `StatusConfig` dataclass (`text: str`) next to
  `NavTarget`/`DisplayConfig` (~line 125-143). `App` (line 174) gets
  `status: Optional[StatusConfig] = None` and `status_bar:
  Optional[StatusBar] = None` (populated post-layout, never authored —
  same relationship `nav_bar` has to `nav`). `StatusBar` carries `rect:
  Rect` + `text: str`.
- **`janus/stage1_parse/dsl_yaml.py`**: parse `status` next to the `nav`
  handling at line 348 (`nav_data = data.get("nav")`). Validation: `status`
  set but `display.size` unset → `ValueError` (mirrors the existing `nav`
  + `display.size` check).
- **`janus/stage2_layout/layout.py`**: `STATUS_BAR_H = 20` module
  constant, next to `NAV_BAR_H = 28` (line 16). `layout_screen` (line 44)
  gains a `has_status: bool = False` param; its `top` calc (line 57)
  becomes `top = (STATUS_BAR_H if has_status else 0) + (NAV_BAR_H if
  has_nav else 0)`, and `avail_h` shrinks by the same amount — default
  `False` so every existing call and every no-status screen lays out
  byte-identical. `build_nav_bar` (line 80, its `rect=Rect(x=x, y=0, ...)`
  at line 99) offsets tab `y` to `STATUS_BAR_H if has_status else 0`. A
  new `build_status_bar(app) -> StatusBar | None` (mirrors
  `build_nav_bar`'s shape): one full-width rect at `y=0`, `h=STATUS_BAR_H`,
  carrying `app.status.text`.
- **`janus/stage3b_embedded_c/emit_embedded_c.py`**: `emit_app_table`
  (line 630) bakes `janus_app_status_bar` — a `static const
  janus_status_bar_t janus_app_status_bar JANUS_PROGMEM` (rect + flash
  text ptr), analogous to the `janus_app_nav_tabs[]` baking at line
  655-693 — plus `janus_app_t.status_bar` (pointer, `NULL` when `status`
  unset). The `JANUS_DISPLAY_PANEL_W/H` emission (line 612-618) extends
  its trigger condition to fire when `status` is set too, not only
  `nav`/`background`.
- **`runtime/embedded_c/include/janus_runtime.h`**: `janus_status_bar_t`
  struct (`rect` + flash text ptr) + the new `janus_app_t.status_bar`
  field. No behaviour.
- **Docs**: `architecture.md` Stage 1 (`status` field), Stage 2 (the
  band + `build_status_bar`), Stage 3b (the descriptor); `Janus.md`'s
  field catalog gets a `status` entry.

### Not in scope

- `draw_status_bar` / any pixels, and all call-site wiring (task 2).
- Bound/live status text (epic out-of-scope).

## Dependencies

Epic decisions 1-4 are settled (see `epic.md`, 2026-09-15): `status` is
app-level only; `display.size` becomes required with `status`, same as
`nav`; `STATUS_BAR_H = 20`; the status band is the true top, with the
nav strip (if present) shifting down under it.

## Pre-work

- A `tests/fixtures/app_with_status.yaml` (+ minimal screen) exercising
  `status` alone and combined with `nav`. Plain YAML, no code — planning
  pre-work.

## Tests

- `tests/test_layout.py`: status alone → root at `y = STATUS_BAR_H`;
  status + nav → root at `y = STATUS_BAR_H + NAV_BAR_H` and
  `build_nav_bar`'s tabs at `y = STATUS_BAR_H`; neither set → both
  unchanged from today.
- `tests/test_dsl_yaml_app.py`: `status` without `display.size` raises.
- `tests/test_emit_embedded_c_app.py` / `test_emit_files.py`: the
  `janus_app_status_bar` descriptor appears with the right rect/text;
  `status_bar == NULL` when `status` is unset.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green (no runtime behaviour changed — struct only) ·
`scripts/avr_gate.sh` green · docs updated · this file `git mv`-ed to
`1-status-bar-field-and-layout-done.md` · commit + merge
`1-status-bar-field-and-layout → tasks`.
