# Task 2: shade-line-trig

## Contract

Add the three remaining primitives: a vertical two-stop shade, an integer
line, and a fixed-point sine table. `led` shading (kind_visuals 3) and
the `vu` needle (vu_meter 2) are the consumers.

### In

Nothing authored — internal runtime helpers only.

### Delivered

`runtime/embedded_c/src/janus_runtime.c`:

- `static uint16_t rgb565_lerp(uint16_t a, uint16_t b, uint8_t t);`
  - unpack R5/G6/B5 of both, linear-interpolate each channel by `t/255`,
    repack. Integer only.
- `static void shade_rect_v(janus_rect_t rect, uint16_t top, uint16_t bottom);`
  - one `fill_rect` per row (`rect.h` spans), row `i` filled with
    `rgb565_lerp(top, bottom, i * 255 / (rect.h - 1))`. `rect.h <= 1` →
    single `fill_rect(rect, top)`.
  - async-safe (only calls `fill_rect`).
- `static void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t colour);`
  - integer Bresenham; each plotted pixel is a `fill_rect` of `{x, y, 1, 1}`
    (keeps it async-safe and clip-safe with zero new machinery — fine at
    the tick/needle scale this is used for).

`runtime/embedded_c/src/janus_font.c` **not** touched — the table goes in
`janus_runtime.c` (or a new `janus_trig.h`/`.c` pair if it reads
cleaner; implementer's call, keep it one concern per file).

- `static const int16_t janus_sin_q15[91] JANUS_PROGMEM = { ... };`
  quarter-wave, degrees 0..90, Q15 (`sin * 32767`), read with
  `JANUS_PGM_READ_U16`.
- `static int16_t janus_sin16(int16_t deg);` / `static int16_t janus_cos16(int16_t deg);`
  - reduce `deg` mod 360, fold the quadrant onto the 0..90 table, sign as
    appropriate. `janus_cos16(d) == janus_sin16(d + 90)`.

### Not in scope

- A Q15 multiply helper beyond what the needle math needs inline — the
  needle endpoint is `pivot + (len * janus_cos16(a) >> 15)`, computed at
  the call site in vu_meter task 2.
- Horizontal shade — no consumer yet.
- Bothering to make `draw_line` fast (run-length spans for shallow
  slopes) — pixel-at-a-time is fine for ticks + one needle.

## Dependencies

None hard. Lands after task 1 only for a clean single merge order into
`tasks`; no code dependency on `fill_rounded_rect`/`fill_circle`.

## Pre-work

None.

## Tests (`runtime/embedded_c/tests/test_runtime.c`)

- `rgb565_lerp(0x0000, 0xffff, 0)   == 0x0000`;
  `rgb565_lerp(0x0000, 0xffff, 255) == 0xffff`;
  `rgb565_lerp(x, x, 128)           == x`.
- `shade_rect_v` over a 1x10 rect: mock-log row 0 colour == `top`,
  row 9 colour == `bottom`, and the sequence is monotone per channel.
- `draw_line(2,2, 2,8)` writes `(2,2)`..`(2,8)` inclusive, contiguous,
  nothing off-column.
- `draw_line(0,0, 5,5)` writes `(0,0)` and `(5,5)`.
- `janus_sin16(0) == 0`; `janus_sin16(90) == 32767`; `|janus_sin16(30) - 16384| <= 1`;
  `janus_cos16(0) == 32767`; `janus_sin16(-90) == -32767`.

## DoD

Contract delivered · `ctest` green · `scripts/avr_gate.sh` green (table
in flash, no new `.bss`) · no Python file touched · this file renamed
`2-shade-line-trig-done.md` · commit + merge `2-shade-line-trig → tasks`.
