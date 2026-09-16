# Epic: status_bar

Render an app-level status band as Janus-owned chrome — a fixed-height
row above the nav strip (true top of the panel), baked once from a new
`app.yaml` field and painted by the fixed runtime — replacing the
per-screen hand-authored `status_bar` row every consumer currently
writes by hand.

## Motivation

Scoped from `initiatives/janus_handoff/README.md` item 2 (2026-09-15,
ArduinoIHM session): today every screen renders
`[nav strip] → [status_bar row] → [screen content]`, and the status row
is just an ordinary per-screen widget — `layout.py`'s
`layout_screen()` unconditionally reserves the nav band at `y = 0` with
no way to push a screen's own content (including a `status_bar` row)
above it. ArduinoIHM's three `.screen.yaml` files each hand-author an
*identical* placeholder status row, which already caused a real bug
(silent duplicate-key row drop, see `initiative.md`) — a strong signal
this belongs at the app level, not per-screen, same as `nav_tabs`.

## Decisions (settled with Rafael 2026-09-15 — these are fixed constraints for the tasks below)

1. **Status is app-level only, never a per-screen widget.** A new
   optional `app.yaml` block, `status: { text: "..." }`, parsed in
   Stage 1 next to `nav`/`display`/`input` (same spot `nav_data =
   data.get("nav")` is handled, `janus/stage1_parse/dsl_yaml.py:348`).
   `App` (`janus/ir.py:174`) gets `status: Optional[StatusConfig] =
   None`; static text only for v1 (see "Out of scope" — no bound field).
2. **`display.size` is required when `status` is set** — same reasoning
   as `nav` decision 1 (`nav_tabs` epic): the band needs the panel width
   to paint full-width, and the `JANUS_DISPLAY_PANEL_W/H` seam
   (`emit_embedded_c.py` ~line 612-618) already exists for exactly this,
   just needs `status` added to its trigger condition alongside
   `nav`/`background`.
3. **`STATUS_BAR_H = 20`**, a fixed Janus layout constant
   (`janus/stage2_layout/layout.py`, alongside `NAV_BAR_H = 28` at line
   16) — one line of `medium` text (`JANUS_FONT_MEDIUM_GLYPH_H` = 14)
   plus ~3px padding top/bottom. `JANUS_COLOR_STATUS_BG` (`0x18e3`) /
   `JANUS_COLOR_STATUS_INK` (`0xffff`) fixed runtime constants
   (`runtime/embedded_c/src/janus_runtime.c`, alongside
   `JANUS_COLOR_NAV_*` at line 1079) — reuses the nav strip's own
   inactive-bg/active-label pairing so the two bands read as one chrome
   family rather than clashing. Like `NAV_BAR_H`/`JANUS_COLOR_NAV_*`,
   these are Janus constants, not authored styling — revisit only if a
   real project needs it.
4. **The status band is the true top of the panel; the nav strip shifts
   down under it, not the other way round.** `layout_screen`'s `top =
   NAV_BAR_H if has_nav else 0` (`layout.py:57`) becomes `top =
   (STATUS_BAR_H if has_status else 0) + (NAV_BAR_H if has_nav else 0)`;
   `build_nav_bar`'s tab rects (`layout.py:99`, currently `y=0`) move to
   `y = STATUS_BAR_H if has_status else 0`. `status` can be set with or
   without `nav` — a project with no tabs still gets a top-most status
   band if it declares one.
5. **Two tasks, not three.** Unlike `nav_tabs`, the status band is
   display-only — no touch hit-test, no focus/encoder interaction, no
   Stage 6 work at all. "Wiring" is light enough (the same call sites
   `draw_nav_bar` already uses) to fold into the draw task instead of
   standing alone.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-status-bar-field-and-layout-done.md` | ✅ done | Stage 1 parses `app.yaml status:`; Stage 2 reserves `STATUS_BAR_H` as the true top band; Stage 3b bakes a status descriptor. Data only, nothing draws. |
| 2 | `tasks/2-draw-and-wire-status-bar.md` | ⬜ planned, depends on task 1 | `draw_status_bar` in the fixed runtime, wired into the same call sites `draw_nav_bar` uses (`janus_switch_screen[_async_start]`, all 6 scaffold templates). |

## Acceptance gate

1. `examples/host_demo` renders a status band as the true top-most band
   on the panel (above the nav strip, if any), with no hand-authored
   `status_bar` row in any `.screen.yaml`.
2. The band is not repainted on the periodic render tick — only on an
   actual screen change or scaffold startup.
3. Python `unittest` + C `ctest` green on host; `scripts/avr_gate.sh`
   green.

## Out of scope

- Bound/live status text — v1 is static text only (the handoff's own
  reasoning: nothing in the current placeholder is a stand-in for
  something already wired; add the binding path when something actually
  needs it, not speculatively).
- Removing ArduinoIHM's three hand-authored `status_bar` rows — that's a
  follow-up in that project once this epic lands, not part of this work.
