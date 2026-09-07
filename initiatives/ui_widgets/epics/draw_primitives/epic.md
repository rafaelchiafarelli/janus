# Epic: draw_primitives

Give `janus_runtime.c` the allocation-free shape helpers the other three
epics need, without adding a new async op kind. Every helper here
ultimately decomposes into `fill_rect` calls (uniform-colour spans), so
`g_async_enqueue` continues to capture them as `JANUS_ASYNC_OP_FILL` ops
for free — nothing about the non-blocking path changes.

## Contract

New `static` helpers in `janus_runtime.c` (declared near `fill_rect` /
`fill_rect_fraction`), plus a small const `sin16` table. Pure additions —
no existing function signature or descriptor field changes. All helpers
clip to the passed rect / to non-negative w,h the same way `fill_rect`
already does.

## Tasks

| # | file | contract (one line) |
|---|---|---|
| 1 | `tasks/1-spans-and-shapes.md` | `fill_rounded_rect(rect, radius, colour)` + `fill_circle(cx, cy, r, colour)` — scanline-decomposed to `fill_rect`, clip-safe, async-safe |
| 2 | `tasks/2-shade-line-trig.md` | `shade_rect_v(rect, top, bottom)` RGB565 per-row lerp + `draw_line(x0,y0,x1,y1,colour)` integer Bresenham + `janus_sin16`/`janus_cos16` Q15 LUT (no libm) |

## Acceptance gate

- `ctest` green, including new `test_runtime.c` cases that assert, via the
  mock driver's pixel log: a rounded rect leaves its four corner pixels
  unfilled at `radius > 0`; a circle's centre pixel is filled and a
  pixel outside `r` is not; `shade_rect_v` top row == `top` and bottom
  row == `bottom`; `draw_line` hits both endpoints and is contiguous;
  `janus_sin16(0)==0`, `janus_sin16(90°)==32767` (±1).
- `scripts/avr_gate.sh` green — helpers add no `.bss`, and `sin16` lives
  in `JANUS_PROGMEM` (read via `JANUS_PGM_READ_U16`), not RAM.
- No change to any generated file or Python test (runtime-only epic).

## Out of scope

- Anti-aliasing / sub-pixel anything — hard-edged fills only.
- A general polygon or arc-fill primitive — the needle scale in
  `vu_meter` draws ticks as short `draw_line` segments, it does not need
  a filled arc.
- Exposing any of these in `janus_runtime.h`'s public API — they stay
  `static` to the .c file until a second translation unit needs them.
