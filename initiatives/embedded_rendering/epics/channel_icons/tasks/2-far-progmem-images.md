# Task 2: far-progmem-images

Status: **ready** — re-scoped 2026-09-06 after the `__memx` plan failed
verification (see below).

## Why the plan changed

The original contract said `image_pixels` becomes a `const __memx
uint16_t *` and a plain array symbol promotes to it in a static
initializer. Verified on `avr-gcc -mmcu=atmega2560`: **it does not** —
`initializer element is not computable at load time`. `__memx` tagged
pointers, like `pgm_get_far_address`, can only be formed at runtime.
`__flash1..5` banked pointers *can* static-init but need a linker script
to actually place the data in their bank (didn't, in a test — data stayed
in low `.text`), so they're unsafe without build-config changes too.

Conclusion: any working design resolves far addresses **at runtime**.

## Contract (v2)

Baked RGB565 arrays stay ordinary `JANUS_PROGMEM`. The widget descriptor
holds a 1-based *slot index*, not a pointer. Each screen carries a tiny
RAM table of `uint_farptr_t` and a generated function that fills it (via
`pgm_get_far_address`, legal in a function body); the fixed runtime calls
that function when the screen becomes current and blits with `memcpy_PF`.

### Delivered

**`runtime/embedded_c/include/janus_progmem.h`** — new, AVR vs host:
- `janus_farptr_t` — `uint_farptr_t` (AVR) / `const uint16_t *` (host)
- `JANUS_FAR_ADDR(sym)` — `pgm_get_far_address(sym)` / `(sym)`.
  **Runtime-only** (function bodies), never a static initializer.
- `JANUS_FAR_ADD(base, byte_off)` — far pointer + byte offset, both sides
- `JANUS_MEMCPY_PF(dst, far, n)` — `memcpy_PF` / `memcpy`

**`runtime/embedded_c/include/janus_runtime.h`**:
- `janus_widget_desc_t`: `const uint16_t *image_pixels` → `uint16_t
  image_slot` (**1-based**; 0 = not an image / no `file:`, so a
  zero-initialised descriptor is safe). `image_w/h/error` unchanged.
- `janus_screen_desc_t`: `+ void (*resolve_images)(void)` and
  `+ const janus_farptr_t *image_far` (both NULL when the screen bakes no
  image).

**`runtime/embedded_c/src/janus_runtime.c`**:
- `static const janus_farptr_t *g_image_far` next to `g_current_screen`.
- `janus_render_screen` and `janus_render_screen_if_dirty` (the two
  screen-entry chokepoints — `switch_screen` / `async_start` both route
  through the former): after `janus_screen_load`, `if (ls.resolve_images)
  ls.resolve_images(); g_image_far = ls.image_far;`
- `janus_async_op_t.img` and `async_enqueue_image`'s param → `janus_farptr_t`.
- `blit_image`'s `src` param → `janus_farptr_t`; the per-row
  `JANUS_MEMCPY_P` (sync path and async-drain path) → `JANUS_MEMCPY_PF` +
  `JANUS_FAR_ADD` for the row offset.
- `draw_image`: `if (lw.image_slot == 0 || g_image_far == NULL)` → stub
  `fill_rect(color)`; else `blit_image(geo, g_image_far[lw.image_slot - 1],
  w, h)`. `image_error` still checked first.
- `janus_render_widget` docstring: gains a line that an image widget
  refreshed on its own only blits if a full screen render for that screen
  ran first (else it falls back to the colour stub — graceful, not a
  crash).

**`janus/stage3b_embedded_c/emit_embedded_c.py`**:
- `_image_fields_c` takes an `image_pxs: list[str]` accumulator (threaded
  from `emit_screen` through `_emit_widget` / `_widget_init`). A baked
  array appends its `_px` name and the fragment becomes
  `.image_slot = <len>` (1-based). No-file / non-image / error →
  `.image_slot = 0`.
- `emit_screen`: after the widgets array, if `image_pxs` is non-empty,
  emit `static janus_farptr_t <sv>_img_far[N];` + `static void
  <sv>_resolve_images(void){ <sv>_img_far[i] = JANUS_FAR_ADDR(<name_i>);
  ... }`, and the screen-desc initializer gets `.resolve_images` /
  `.image_far`; otherwise both `NULL`.

**Tests**: `runtime/embedded_c/tests/test_runtime.c` (fixtures 9, 9c) and
`test_render_async.c` (image fixture) — hoist the pixel arrays to file
scope, add a one-line resolver + `img_far[1]` table, set `.image_slot =
1` and the two screen-desc fields. `tests/test_emit_embedded_c*.py` —
update expected substrings (`image_slot`, `resolve_images`, `image_far`).
Add one Python test: a screen with 2 baked images emits a 2-entry
`_img_far` table + a `_resolve_images` with 2 `JANUS_FAR_ADDR` lines, and
the two image widgets carry `.image_slot = 1` / `.image_slot = 2`.

**Docs**: `architecture.md` Stage 3b (image slot + per-screen resolver)
and Stage 4 (far blit); `Janus.md` RGB565/PROGMEM section + `image`
catalog row.

### Verification (recorded in the commit)

- `python -m unittest discover -s tests` green.
- `runtime/embedded_c` `ctest` green on host.
- `avr-gcc -mmcu=atmega2560 -Os -c` on every `examples/host_demo`
  generated `.gen.c` + `runtime/embedded_c/src/*.c` against the vendored
  headers: **no** `value of NNNNN too large for field of 2 bytes`, no
  address-space diagnostic. `avr-size -A` to confirm the `_px` arrays are
  what sits past 64 KiB and the descriptors / font / strings stay under
  it. If they don't, stop and flag — the contract assumed they would.

## Dependencies

None on task 1's code. Uses the row-height-icon
`examples/host_demo/pwm.screen.yaml` already on `dev` (3d4f527).

## Pre-work

None.

## DoD

Contract delivered · Python suite green · `ctest` green · the
`avr-gcc -mmcu=atmega2560` compile clean · docs updated · this file
marked done · commit + merge `2-far-progmem-images → tasks`, then walk
`tasks → channel_icons` (epic acceptance gate) `→ epics →
embedded_rendering → features → dev`.
