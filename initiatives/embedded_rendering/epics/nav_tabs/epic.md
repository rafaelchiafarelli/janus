# Epic: nav_tabs

Render `app.nav: { kind: tabs }` as a real, persistent tab strip Janus
draws itself — one horizontal band across the top of **every** screen,
the current screen's tab visually distinct, wired to actually switch
screens. Today `nav`/`nav_titles` is parsed and emitted but nothing reads
it (`architecture.md` ~line 746: "no tab-bar rendering implemented"), so
every consumer that wants tabs hand-authors a `row` of `button` widgets
per screen — which is what ArduinoIHM does, and why "the PWM tab is too
wide", "the active tab isn't highlighted", and "the tabs don't show
selection" all landed as bug reports 2026-09-07 that a runtime tweak
can't fix.

## Motivation

- The hand-authored `tab_bar` row only exists on one of ArduinoIHM's
  three screens; the other two have no tab affordance at all.
- Its per-tab `bg:` colours are static YAML — they don't follow the
  active screen, so nothing marks "you are here".
- All tabs are authored at one fixed `w:`, so a long label ("SERIAL")
  clips while a short one ("PWM") floats — Janus should size tab cells
  uniformly from the panel width and tab count.
- `nav` is already the right declaration; it just needs a renderer + an
  input path.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-nav-bar-geometry-and-descriptor-done.md` | ✅ **done** | `NAV_BAR_H`=28 band offsets every screen's root when `app.nav` set; Stage 1 requires `display:` + validates tab count/targets; `build_nav_bar` lays equal cells (last absorbs remainder); `janus_nav_tabs[]` + `.nav_tab_count` baked into the app table; `JANUS_DISPLAY_PANEL_W/H` now emitted for `nav` too. No pixels. |
| 2 | `tasks/2-draw-nav-bar-done.md` | ✅ **done** | `janus_render_nav_bar`/`draw_nav_bar` in the fixed runtime — N equal cells (pgm-safe `janus_nav_tab_load`), `medium` centered titles (no shrink), active cell = active fill + 6px accent bar. Wired into `janus_switch_screen[_async_start]` + all 6 `main_*.c.tmpl` scaffolds. Not in `janus_render_screen`. |
| 3 | `tasks/3-nav-bar-input-done.md` | ✅ **done** | `janus_nav_hit_test` (tap in a cell → `NAVIGATE`) wired into the touch scaffolds; public `janus_nav_next/prev(app)` cycle helper; docs. The focus-stop half of decision 3 split out ↓. |
| 4 | `tasks/4-nav-bar-focus-stop.md` | ⬜ planned | Fold the strip into `janus_input_focus.c` traversal so a single-control encoder/button scaffold reaches the tabs — needs `app` threaded through the focus API + a synthetic focus position + a focus ring on a cell rect. Split from task 3 (invasive to the focus core). |

## Decisions (settled with Rafael 2026-09-07 — these are fixed constraints for the tasks below)

1. **`display.size` is required when `app.nav` is set.** Stage 1 raises
   `ValueError` if `nav` is present and `display.size` is not. Panel width
   reaches the runtime via the `janus_render_config.gen.h` seam
   `display.background` established (`JANUS_DISPLAY_PANEL_W/H`).
2. **`NAV_BAR_H` and styling are fixed Janus constants for v1** — no
   `nav:` styling fields. Like `BOX_HEADER_H` / `JANUS_FOCUS_RING_W`:
   `NAV_BAR_H` a layout constant, `JANUS_COLOR_NAV_*` runtime constants.
   Active cell = accent fill + a ~6px bottom accent bar; inactive =
   outline on the inactive bg. Revisit theming only if a real project
   needs it.
3. **Both non-touch interaction paths.** A dedicated
   `janus_nav_next(app)` / `janus_nav_prev(app)` helper (scaffold binds a
   control to it — mirrors ArduinoIHM's rot0-drives-tabs) **and** the
   strip is a focus stop in the per-screen focus order (rotate-while-
   focused cycles tabs, click commits). Both land in
   `janus_switch_screen` + `janus_render_nav_bar`.
4. **The nav strip is a separate band above everything.** `NAV_BAR_H` at
   `y = 0`; the screen (including its own authored `header`/`status_bar`
   row, unchanged) starts at `y = NAV_BAR_H`. Screens keep authoring
   their header rows exactly as today — the strip does not replace that
   convention.

## Status

Tasks 1–3 done and **fast-tracked to `dev` 2026-09-07** (Rafael's call —
the epic reaches `dev` incrementally rather than as one unit). Task 4
(focus-stop) is the only work left; branch it off `dev`. The epic's own
`epics`→`dev` merge already happened with the fast-track, so task 4 lands
via its own `<task> → tasks → nav_tabs → … → dev` walk when done.

## Acceptance gate

1. `examples/host_demo` (already declares `nav: { kind: tabs }`) renders a
   tab strip on all three screens with the active one distinct, no
   hand-authored tab `row` in any `.screen.yaml`. *(host_demo's frozen
   scaffolded `src/main.c` needs a one-line `janus_render_nav_bar` /
   `janus_nav_hit_test` hand-edit — human-owned once scaffolded; do it
   with task 4.)*
2. A touch/tap in a tab cell navigates to that screen ✅ (task 3); the
   single-control encoder/button path switches tabs — **task 4**.
3. `scripts/avr_gate.sh` green (SRAM/flash budget holds), Python
   `unittest` + C `ctest` green on host — ✅ held through task 3.
4. The nav strip is not repainted on the periodic render tick — only on
   an actual screen change ✅ (task 2 — `draw_nav_bar` is out of
   `janus_render_screen` entirely).

## Out of scope

- `nav.kind` other than `tabs` (drawers, bottom bars) — the IR already
  hard-codes `tabs`; leave it.
- Scrolling / overflow when tabs don't fit — v1 assumes they fit the
  panel width (validate and error, don't scroll).
- Animated tab transitions.
- Removing ArduinoIHM's hand-authored rows — that's the ArduinoIHM
  session's follow-up once this lands (see
  `handle-to-janus.md` direction / the follow-up memory).
