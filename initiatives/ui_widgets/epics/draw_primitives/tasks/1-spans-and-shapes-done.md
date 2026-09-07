# Task 1: spans-and-shapes

## Contract

Add two allocation-free shape helpers to `janus_runtime.c`, built only
out of `fill_rect` so both the blocking and the `JANUS_RENDER_NONBLOCKING`
paths capture them with no new code.

### In

Nothing authored — internal runtime helpers only.

### Delivered

`runtime/embedded_c/src/janus_runtime.c`, new `static` functions placed
just after `fill_rect_fraction`:

- `static void fill_rounded_rect(janus_rect_t rect, int16_t radius, uint16_t colour);`
  - `radius <= 0` → exactly `fill_rect(rect, colour)`.
  - `radius` is clamped to `min(rect.w, rect.h) / 2`.
  - Implementation: one `fill_rect` for the centre band
    (`rect` inset by `radius` top and bottom is the full width; the
    middle rows are full width), then for each of the `radius` top rows
    and `radius` bottom rows emit a single horizontal `fill_rect` span
    inset by `radius - isqrt(radius*radius - dy*dy)` on each side. Integer
    `isqrt` (local `static` helper, Newton or bit-by-bit — no `math.h`).
  - Net: `2*radius + 1` `fill_rect` calls, each a uniform span.
- `static void fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour);`
  - `r <= 0` → no-op.
  - Per-row (`dy` from `-r` to `r`) one horizontal `fill_rect` span of
    half-width `isqrt(r*r - dy*dy)` centred on `cx`. `2*r + 1` spans.
  - Clips like `fill_rect` (negative origins / widths are already
    guarded there).

Both must work when `g_async_enqueue` is set — since they only call
`fill_rect`, verify (test) that an async render of a widget using them
enqueues `JANUS_ASYNC_OP_FILL` ops and nothing else.

### Not in scope

- `fill_circle` outline-only / ring variant — `draw_led` (kind_visuals
  task 3) draws its rim as a slightly larger filled disc under the main
  one, it does not need a stroked circle.
- Any public (`janus_runtime.h`) declaration.

## Dependencies

None. Self-contained against the current runtime.

## Pre-work

None. New `test_runtime.c` cases use the existing mock driver + its
pixel log (`mock_driver.h`), no new fixtures.

## Tests (`runtime/embedded_c/tests/test_runtime.c`)

- `fill_rounded_rect` with `radius == 0` produces the identical mock-log
  fill sequence as `fill_rect` of the same rect.
- `fill_rounded_rect` with `radius == 4` on a 20x20 rect: the corner
  pixel `(x, y)` is never written; the pixel `(x + radius, y)` is.
- `fill_circle(cx, cy, 5)`: `(cx, cy)` written with `colour`; `(cx + 6, cy)`
  never written; `(cx, cy - 5)` written.
- clip: `fill_circle` centred so it straddles `x = 0` writes no negative
  x (no mock-log entry with x < 0).
- async: build a one-widget screen whose draw calls `fill_circle`, run
  `janus_render_screen_async_start` + drain `janus_render_poll`; every
  drained op is `JANUS_ASYNC_OP_FILL`.

## Notes — deviations from the sketch above

- **Not `static`.** A unit test is "a second translation unit that needs
  them", so the two helpers are `extern`, `janus_`-prefixed
  (`janus_fill_rounded_rect` / `janus_fill_circle`), declared in a new
  **internal** header `runtime/embedded_c/include/janus_draw.h` — *not*
  the vendor-facing `janus_runtime.h`, so the "no public declaration"
  constraint still holds. Definitions stay in `janus_runtime.c` next to
  `fill_rect` as specified.
- **Left-clip helper.** `fill_rect` does *not* actually guard negative
  origins (the sketch assumed it did) — widget geometry is always
  on-screen, but a rounded corner / circle can round past x = 0. A tiny
  `fill_hspan` clips `x < 0` (and drops rows above y = 0) before calling
  `fill_rect`. Still "only built out of `fill_rect`".
- **Tests are `tests/test_draw.c`** (new `janus_draw_tests` ctest
  target), not `test_runtime.c` — the helpers are called directly there.
- **Async assertion deferred.** Nothing in the runtime calls these yet
  and `g_async_enqueue` is private to `janus_runtime.c`, so the
  "widget → async_start → every op is FILL" check moves to
  **kind_visuals task 1** (toggle), where a real widget exercises them
  through the queued path. Async-safety is structural meanwhile: the only
  driver-touching call underneath is `fill_rect`.

## DoD

Contract delivered · `ctest` green (9/9, incl. `janus_draw_tests`) ·
`scripts/avr_gate.sh` green · no Python file touched · this file renamed
`1-spans-and-shapes-done.md` · commit + merge `1-spans-and-shapes → tasks`.
