# Epic: kind_visuals

Replace the flat solid-fill renders for `toggle`, `progress`+`gauge`, and
`led` with ones that read as their kind. Runtime-only — no DSL, no Stage
3b, no descriptor changes; each task rewrites one `draw_<kind>` function
using the `draw_primitives` helpers. Widgets keep drawing from their own
`.color` (on / ink / fill) and `.bg_color` (off / track / face) exactly
as the catalog already documents — no new authored fields.

## Dependency

**draw_primitives** epic merged to `epics` first (all three tasks use
`fill_rounded_rect` / `fill_circle`; task 3 also uses `shade_rect_v`).

## Tasks

| # | file | contract (one line) |
|---|---|---|
| 1 | `tasks/1-toggle-switch.md` | `draw_toggle` → rounded track + circular knob that sits left (off) / right (on) by the bound value |
| 2 | `tasks/2-progress-gauge-bar.md` | `draw_progress_or_gauge` → recessed rounded track + rounded proportional fill + 1px top shade; both kinds share it |
| 3 | `tasks/3-led-shaded.md` | `draw_led` → filled disc with a darker rim ring and a lighter specular highlight; warn state still amber |

## Acceptance gate

- `ctest` green — each task adds `test_runtime.c` assertions against the
  mock pixel log (knob centre x for on vs off; filled span width vs
  fraction and rounded end; led centre vs rim colour per state 0/1/2).
- `scripts/avr_gate.sh` green — no `.bss` growth (`draw_*` stay
  stack-only), `blocking` `host_demo` still links for atmega2560.
- The `JANUS_RENDER_NONBLOCKING` `test_render_async.c` still passes —
  every new draw path decomposes to `fill_rect`, so async capture is
  unchanged.
- `checkbox` / `badge` / `slider` renders are **untouched** — only
  `toggle`, `progress`, `gauge`, `led` change.

## Out of scope

- A true arc/dial `gauge` — it takes the same bar look as `progress`
  (initiative decision). A distinct radial gauge is a later item.
- Making `toggle` interactive / animated between states — it snaps to the
  current bound value, same as every other display-only widget.
- Per-widget authorable rim/highlight colours for `led` — derived from
  `.color` by `rgb565_lerp` toward black / white.
