# Initiative: app_chrome

App-level chrome Janus bakes and renders itself, above any single
screen's own content — the same relationship `nav_tabs` established
between `app.yaml`'s `nav:` block and the fixed runtime's
`draw_nav_bar`. Scope is chrome bands that sit outside every screen's
`children`, not new widget kinds or per-screen layout.

## Motivation

`nav_tabs` (under the now-closed `embedded_rendering` initiative) proved
the pattern: declare app-level chrome once in `app.yaml`, bake it in
Stage 2/3b, render it from the fixed runtime — instead of every consumer
hand-authoring the same row on every screen and drifting out of sync.
ArduinoIHM hit exactly that drift for its status row (identical
placeholder text duplicated across all three `.screen.yaml` files, and a
copy-paste duplicate `children:` key silently dropped one screen's row
entirely — YAML keeps only the last of two identical keys, no parse
error) and asked, via `initiatives/janus_handoff/README.md` (2026-09-15),
for Janus to own it the same way it owns the nav strip.

## Epics

- **status_bar** — render an app-level status band above the nav strip
  (or at the very top, if there's no nav), replacing the per-screen
  hand-authored `status_bar` row.

## Cross-epic gate

`examples/host_demo` and the Python/`ctest` host suites stay green with
the new chrome band in place; `scripts/avr_gate.sh` holds budget.
