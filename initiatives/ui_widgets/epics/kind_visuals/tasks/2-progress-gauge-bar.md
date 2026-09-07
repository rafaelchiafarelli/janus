# Task 2: progress-gauge-bar

## Contract

Rewrite `draw_progress_or_gauge` in `janus_runtime.c` to render a real
bar: a recessed rounded track, a rounded proportional fill, and a 1px
shade highlight along the top. `progress` and `gauge` keep sharing this
one function (initiative decision — no distinct arc gauge in this
initiative).

### In

No authoring change. `range: {min, max}` is still required by Stage 1;
the fraction math is unchanged.

### Delivered

`runtime/embedded_c/src/janus_runtime.c` — `draw_progress_or_gauge`:

- `fraction` computed exactly as today (`(value - range_min) / span`,
  clamped 0..1 — keep `fill_rect_fraction`'s clamp behaviour; that helper
  may be inlined/removed if it has no other caller after this).
- track: `fill_rounded_rect(r, r.h/2, lw.bg_color)`.
- fill: `filled_w = (int16_t)(r.w * fraction)`; if `filled_w > 0`,
  `fill_rounded_rect({r.x, r.y, filled_w, r.h}, r.h/2, lw.color)`.
  (rounded on both ends is acceptable at these sizes; a half-round
  left-only variant is explicitly not worth a new primitive.)
- shade: `shade_rect_v({r.x, r.y, r.w, max(1, r.h/3)},
  rgb565_lerp(lw.color, 0xffff, 64), lw.color)` **clipped to the filled
  region** (`w = filled_w`) so only the filled part gets the gloss.
- `filled_w == 0` → just the track (no fill, no shade).
- `fraction == 1` → fill == track rect.

### Not in scope

- `slider` — it also calls `fill_rect_fraction` today; leave
  `draw_slider` on its current flat render (out of this initiative's
  scope list). If `fill_rect_fraction` is removed, give `draw_slider` its
  own 3-line inline flat split so it is genuinely untouched in behaviour.
- A radial / arc gauge.
- Tick marks or a value label on the bar (a formatted `label` beside it
  covers that — see the `label_format` epic).

## Dependencies

- **draw_primitives task 1** (`fill_rounded_rect`) + **task 2**
  (`shade_rect_v`, `rgb565_lerp`) merged to `tasks`; **draw_primitives**
  epic merged to `epics`.

## Pre-work

None.

## Tests (`runtime/embedded_c/tests/test_runtime.c`)

- fraction `0.5` on a 100-wide bar: a `lw.color` pixel exists near x≈40,
  none near x≈80 (allow the corner radius slack).
- fraction `0`: no `lw.color` pixel anywhere in the geometry; track
  colour present at the midline.
- fraction `1`: `lw.color` present at x = r.w - 2 midline.
- rounded end: the track's corner pixel (`r.x, r.y`) is not `lw.bg_color`
  (left unfilled by the rounded rect).
- async render drains only `JANUS_ASYNC_OP_FILL`.
- a `gauge` widget with the same bind/range renders an identical pixel
  log to a `progress` (they share the path).

## DoD

Contract delivered · `ctest` green · `scripts/avr_gate.sh` green · no
Python file touched · this file renamed `2-progress-gauge-bar-done.md` ·
commit + merge `2-progress-gauge-bar → tasks`.
