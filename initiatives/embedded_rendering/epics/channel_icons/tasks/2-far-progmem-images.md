# Task 2: far-progmem-images

Status: **ready** (planning complete, not yet implemented)

## Contract

Baked RGB565 image-pixel arrays are addressed through a 24-bit `__memx`
pointer so they can be linked anywhere in the 256 KiB flash, past the
64 KiB window that ordinary `PROGMEM` pointers and `memcpy_P` reach.
Nothing else about image widgets changes; no consuming-project build
config changes.

### Delivered

- `runtime/embedded_c/include/janus_progmem.h`: new `JANUS_MEMX` macro —
  `__memx` under `__AVR__`, empty otherwise. Short comment: 24-bit
  named-address-space qualifier, valid in static initializers (unlike
  `pgm_get_far_address`), so a generated descriptor can point straight at
  a baked array regardless of where it links.
- `runtime/embedded_c/include/janus_runtime.h`:
  `janus_widget_desc_t.image_pixels` becomes
  `const JANUS_MEMX uint16_t *` (comment updated). `NULL` still assigns
  and compares fine.
- `runtime/embedded_c/src/janus_runtime.c`:
  - `blit_image()` takes `const JANUS_MEMX uint16_t *src`.
  - `janus_async_op_t.img` becomes `const JANUS_MEMX uint16_t *`.
  - There is no `memcpy` from `__memx`, so the per-row `JANUS_MEMCPY_P`
    into `g_tile_buffer` (sync path and async-drain path both) becomes a
    per-pixel copy loop. Host: bit-identical output. AVR: compiler emits
    the far (ELPM/RAMPZ) loads. **Known tradeoff:** per-pixel vs block
    copy is slower on AVR; acceptable for v1, the tiled/async path keeps
    any single poll bounded. Optimizing it (word reads / hand asm) is a
    later task if the board render time needs it.
- `janus/stage3b_embedded_c/emit_embedded_c.py`: `_image_fields_c` — the
  emitted text is unchanged (`.image_pixels = <name>` and
  `.image_pixels = NULL`); the `__memx` qualifier lives only on the
  struct field, and a plain array symbol promotes to it. Confirm no
  change is actually needed here; if a cast turns out necessary, it goes
  here.
- Docs: `architecture.md` Stage 4 image-blit paragraph + Stage 3b image
  note; `Janus.md`'s RGB565/PROGMEM section and the `image` catalog row.

### Verification (run here, recorded in the commit message)

- `python -m unittest discover -s tests` green.
- `runtime/embedded_c` `ctest` green on host (image async test in
  particular).
- `avr-gcc -mmcu=atmega2560 -Os -c` every `examples/host_demo` generated
  `.gen.c` + every `runtime/embedded_c/src/*.c` against the vendored
  headers: **no** `value of NNNNN too large for field of 2 bytes`, no
  address-space warning. `avr-size -A` the objects to show the image
  arrays are the only thing past 64 KiB and the descriptors/font/strings
  stay under it (if they don't, that's a stop-and-flag — the contract
  assumed they would).

## Dependencies

None on task 1's mechanism. (Epic acceptance — two same-slot icons —
needs both tasks, but this one builds and verifies on its own using the
existing single-icon `examples/host_demo/pwm.screen.yaml` already on
`dev`.)

## Pre-work

None. `examples/host_demo/pwm.screen.yaml` already carries row-height
icons (committed on `dev`, 3d4f527).

## DoD

Contract delivered · Python suite green · `ctest` green · the
`avr-gcc -mmcu=atmega2560` compile above clean · docs updated · this file
marked done · commit + merge `2-far-progmem-images → tasks`, then
`tasks → channel_icons → epics → embedded_rendering → features → dev`
once the epic acceptance gate passes.
