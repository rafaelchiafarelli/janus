# Task 3: led-shaded

## Contract

Rewrite `draw_led` in `janus_runtime.c` to render a round, shaded
indicator: a filled disc in the state colour, a darker rim ring, and a
lighter specular highlight toward the top-left.

### In

No authoring change. `states: [off, on, ...]` and the 0/1/warn state
mapping are unchanged — this only changes how the state colour is
painted, not how it is chosen.

### Delivered

`runtime/embedded_c/src/janus_runtime.c` — `draw_led` only:

- state → colour selection unchanged:
  `state <= 0 → lw.bg_color`, `state == 1 → lw.color`,
  `state >= 2 → JANUS_COLOR_LED_WARN`.
- geometry: `cx = r.x + r.w/2`, `cy = r.y + r.h/2`,
  `rad = min(r.w, r.h)/2`.
- fill the widget rect with the *screen* backdrop first? No — keep
  today's behaviour of not painting outside the disc (the container
  already painted the background). Draw:
  1. rim: `fill_circle(cx, cy, rad, rgb565_lerp(colour, 0x0000, 80))`
     (disc darkened ~30% — the ring shows as a 1–2px border under the
     main disc).
  2. face: `fill_circle(cx, cy, rad - 1, colour)`.
  3. highlight: `fill_circle(cx - rad/3, cy - rad/3, max(1, rad/3),
     rgb565_lerp(colour, 0xffff, 130))` (small, up-left, lightened toward
     white).
- the `off` state (`lw.bg_color`, typically near-white) still gets the
  rim + highlight so a dark-on-light theme reads it as a switched-off
  lamp rather than an invisible blob — verify visually in the
  regenerated `host_demo`.

### Not in scope

- Authorable rim / highlight colours (derived via `rgb565_lerp`).
- A `colors:` list parallel to `states:` (noted as a future follow-up in
  `Janus.md` already — not this task).
- Any change to LED hit-testing / focus (LED is display-only, unchanged).

## Dependencies

- **draw_primitives task 1** (`fill_circle`) + **task 2** (`rgb565_lerp`)
  merged to `tasks`; **draw_primitives** epic merged to `epics`.

## Pre-work

None.

## Tests (`runtime/embedded_c/tests/test_runtime.c`)

- state `1`: centre pixel `(cx, cy)` == `lw.color`; a pixel just inside
  the rim (`cx + rad - 1, cy`) is a darkened `lw.color`
  (`!= lw.color` and `!= 0`).
- state `0`: centre pixel == `lw.bg_color`.
- state `2`: centre pixel == `JANUS_COLOR_LED_WARN`.
- highlight: the pixel at `(cx - rad/3, cy - rad/3)` is lighter than
  `lw.color` per-channel (unpack + compare).
- a pixel outside the disc (`cx + rad + 2, cy`) is never written.
- async render drains only `JANUS_ASYNC_OP_FILL`.

## DoD

Contract delivered · `ctest` green · `scripts/avr_gate.sh` green · no
Python file touched · this file renamed `3-led-shaded-done.md` ·
commit + merge `3-led-shaded → tasks`.
