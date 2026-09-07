# Task 1: toggle-switch

## Contract

Rewrite `draw_toggle` in `janus_runtime.c` to render a switch: a rounded
pill track with a circular knob positioned by the bound on/off value.

### In

No authoring change. Existing `toggle` widgets (e.g. `examples/host_demo`
`pwm_ch0_enabled`, all the `relay_*`) pick up the new look automatically.

### Delivered

`runtime/embedded_c/src/janus_runtime.c` — `draw_toggle` only:

- `value = read_bound_value(&lw.bind, bound_struct); on = value != 0.0;`
- let `r = lw.geometry` , `kd = min(r.h, r.w/2)` (knob diameter),
  `pad = 1`.
- track: `fill_rounded_rect(r, r.h/2, on ? lw.color : lw.bg_color)`.
- knob: `fill_circle(cx, r.y + r.h/2, kd/2 - pad, <knob colour>)` where
  `cx = on ? (r.x + r.w - kd/2 - pad) : (r.x + kd/2 + pad)` and the knob
  colour is `rgb565_lerp(track, 0xffff, 96)` (a lightened track) so it
  reads against both states with no new authored field.
- no glyphs, no focus ring (toggle isn't focusable — unchanged).

Keep `draw_checkbox` and `draw_badge` exactly as they are — this is the
point at which `toggle` stops being their visual twin.

### Not in scope

- Animation / tween between positions.
- An authorable knob colour.
- Touching the dispatch/dirty-list entry for `JANUS_WIDGET_TOGGLE` (still
  a bound leaf, still redraws on its dirty bit — no change needed).

## Dependencies

- **draw_primitives task 1** (`fill_rounded_rect`, `fill_circle`) and
  **task 2** (`rgb565_lerp`) merged to `tasks`, and the
  **draw_primitives** epic merged to `epics`.

## Pre-work

None.

## Tests (`runtime/embedded_c/tests/test_runtime.c`)

- toggle bound `0`: some pixel in the left third of the geometry is the
  knob colour; the right third has no knob-colour pixel.
- toggle bound `1`: mirror — knob colour appears only in the right third.
- track colour: with bound `1`, a pixel at the far-left edge midline is
  `lw.color`; with bound `0` it is `lw.bg_color`.
- async: an async render of a screen with one toggle drains only
  `JANUS_ASYNC_OP_FILL` ops.

## DoD

Contract delivered · `ctest` green · `scripts/avr_gate.sh` green · no
Python file touched · this file renamed `1-toggle-switch-done.md` ·
commit + merge `1-toggle-switch → tasks`.
