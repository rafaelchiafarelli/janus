# Epic: channel_icons

Make the PWM screen's full-row-height per-channel mode icons work on the
real Mega2560: an *enabled* and a *disabled* icon variant sharing one
slot, and image data that can live anywhere in the 256 KiB flash.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-hidden-widget-flag.md` | **done** | `hidden: true` widget flag — hidden widget + subtree pruned at generation time |
| 2 | `tasks/2-far-progmem-images.md` | **done (own contract)** | baked RGB565 arrays reachable past 64 KiB via a per-screen resolver + `image_far[]` table + `image_slot` index |
| 3 | `tasks/3-async-path-behind-render-mode.md` | **not scoped** | `g_async_ops[256]` (6912 B SRAM) links into any image-using project even in blocking mode — guard the whole async path behind `render_mode: non_blocking` |
| 4 | `tasks/4-near-progmem-budget.md` | **not scoped** | ~80 KiB of icon arrays displaces the font glyph tables (read with near `pgm_read_byte`) past 64 KiB — far-safe those reads, or guarantee image arrays link last |

Tasks 1 & 2 are independent in mechanism. **Task 2 delivered its own
contract** (verified: `avr-gcc` links `_px` arrays at VMA > 0x10000 and
`memcpy_PF` reads them) but bringing up big icons on the real board
surfaced two more walls (3, 4) that are out of task 2's scope — they need
scoping through the normal process before the epic acceptance gate can
pass.

## Acceptance gate

1. A `.screen.yaml` with two `image` widgets in one slot, one
   `hidden: true`, generates with only the visible one in the output.
2. `examples/host_demo` regenerated with row-height icons:
   `avr-gcc -mmcu=atmega2560 -Os` compiles every generated `.gen.c` + the
   vendored runtime with **no** `value of NNNNN too large for field of 2
   bytes` and no near/far addressing warning on the image path.
3. Python `unittest` suite and C `ctest` suite both green on host.

## Out of scope

- Binding `hidden` to a field (runtime toggle) — separate future task.
- Far-`PROGMEM` for anything other than image pixel data (descriptors,
  strings, font, screen tables stay near; verified they still fit).
- Image compression / palette formats.
