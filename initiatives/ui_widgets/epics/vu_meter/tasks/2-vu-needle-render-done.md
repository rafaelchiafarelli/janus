# Task 2: vu-needle-render

## Contract

Fill in `draw_vu` in `janus_runtime.c`: face + a 90° tick arc + a needle
that pivots from a bottom-centre hub, its angle set by the bound value
within `range`.

### In

No authoring change beyond what task 1 established (`kind: vu`, numeric
bind, `range`, `size`).

### Delivered

`runtime/embedded_c/src/janus_runtime.c` — `draw_vu(w, bound_struct)`:

- `value = read_bound_value(&lw.bind, bound_struct);`
  `span = range_max - range_min;`
  `frac = span != 0 ? clamp((value - range_min)/span, 0, 1) : 0;`
- geometry: `hub_x = r.x + r.w/2`, `hub_y = r.y + r.h - 1`,
  `len = min(r.w/2, r.h) - 2`.
- angle: `deg = -45 + (int16_t)(frac * 90)` (0 == straight up;
  negative == left).
- face: `fill_rect(r, lw.bg_color)`.
- tick arc: 5 ticks at `deg_i = -45 + i*22` for `i` in `0..4`; each tick
  a `draw_line` from `0.9*len` to `1.0*len` along `deg_i`:
  `tx = hub_x + (int32_t)L * janus_sin16(deg_i) >> 15`,
  `ty = hub_y - (int32_t)L * janus_cos16(deg_i) >> 15`
  (note `-cos` for screen-y-down; `sin` for x so +deg goes right).
  Ticks drawn in `lw.color`.
- needle: `draw_line(hub_x, hub_y, hub_x + (L*sin16(deg)>>15),
  hub_y - (L*cos16(deg)>>15), lw.color)` with `L = len`.
- hub: `fill_circle(hub_x, hub_y, max(2, len/10), lw.color)` drawn last so
  it caps the needle root.
- all of face/ticks/needle/hub decompose to `fill_rect` (`draw_line`
  plots 1x1 `fill_rect`s per draw_primitives task 2), so the async path
  captures it with no new op kind — verify in `test_render_async` style.

### Not in scope

- Colour zones, redline, peak hold, damping.
- Sub-pixel / anti-aliased needle — 1px Bresenham is the v1 look.
- A filled scale arc between the ticks (ticks only).
- Making `len`/tick-count/angle-span authorable — fixed in v1.

## Dependencies

- **vu_meter task 1** (the `vu` kind + `draw_vu` stub + dispatch) merged
  to `tasks`.
- **draw_primitives task 2** (`draw_line`, `janus_sin16`, `janus_cos16`)
  and **task 1** (`fill_circle`) merged to `tasks`; **draw_primitives**
  epic merged to `epics`.

## Pre-work

None.

## Tests (`runtime/embedded_c/tests/test_runtime.c`)

- `value == range_min`: the needle's far endpoint (scan the mock pixel
  log for the `lw.color` pixel farthest from the hub) has
  `x < hub_x` and `y < hub_y` (up-left).
- `value == range_max`: far endpoint `x > hub_x`, `y < hub_y` (up-right).
- midpoint: far endpoint `|x - hub_x| <= 2` and `y < hub_y` (≈straight
  up).
- clamp: `value` above `range_max` renders identically to `value ==
  range_max`.
- hub: `(hub_x, hub_y)` region has `lw.color` pixels for every value.
- async render of a one-`vu` screen drains only `JANUS_ASYNC_OP_FILL`.

## DoD

Contract delivered · `python -m unittest discover -s tests` green
(unchanged — no Python touched) · `ctest` green · `scripts/avr_gate.sh`
green · this file renamed `2-vu-needle-render-done.md` · commit + merge
`2-vu-needle-render → tasks`.
