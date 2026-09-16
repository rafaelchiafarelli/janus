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

## Proposed shape (not yet settled with Rafael — confirm before task 1 starts)

Carried over from the handoff doc's "suggested shape," as a starting
point only:

1. A new optional `app.yaml` block, e.g. `status: { text: "..." }`
   (static text only for v1 — no bound field; see "Out of scope").
2. Stage 2's `layout_screen` reserves a second fixed-height band
   *above* the nav band when `status` is set: `top = STATUS_BAR_H +
   (NAV_BAR_H if has_nav else 0)`. `build_nav_bar` bakes the nav strip's
   own `y` at `STATUS_BAR_H` instead of `0` when both are present.
3. A fixed-runtime `draw_status_bar(app)` alongside `draw_nav_bar`,
   called from the same call sites (`janus_switch_screen`, once at
   scaffold startup) — never from the periodic render tick (same
   "chrome repaints on screen-change, not per-frame" rule `nav_tabs`
   established, and the reason ArduinoIHM's own status-bar flicker bug —
   see `janus_handoff`, item 3 — was a *consumer*-side mistake, not
   something Janus should make easy to repeat).

## Open questions to settle with Rafael before task 1

- Does `status` require `display.size`, same as `nav` does (decision 1
  of the `nav_tabs` epic)? Almost certainly yes, for the same reason
  (the band needs the panel width to paint full-width) — but that's a
  DSL constraint, not this session's call to make alone.
- Exact `STATUS_BAR_H` value and colour constants (mirrors
  `NAV_BAR_H` / `JANUS_COLOR_NAV_*` as fixed Janus constants, not
  authored styling — same precedent, needs the same explicit sign-off).
- Field name/shape: `status: { text: "..." }` vs. folding into an
  existing block vs. something else entirely.
- Whether `status` can be set without `nav` (band alone, no tab strip)
  — the "true top-most band" framing in the handoff implies yes.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-status-bar-field-and-layout.md` | ⬜ blocked on open questions above | Stage 1 parses `app.yaml status:`; Stage 2 reserves `STATUS_BAR_H` above the nav band (or at `y = 0` with no nav); Stage 3b bakes a status descriptor. Data only, nothing draws. |
| 2 | `tasks/2-draw-status-bar.md` | ⬜ planned, depends on task 1 | `draw_status_bar` in the fixed runtime — static text centered in the band. |
| 3 | `tasks/3-status-bar-wiring.md` | ⬜ planned, depends on task 2 | Call sites (`janus_switch_screen`, scaffold startup) wired, same pattern as `draw_nav_bar`; `architecture.md` updated. |

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
