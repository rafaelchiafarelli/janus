# Epic: channel_icons

Make the PWM screen's full-row-height per-channel mode icons work on the
real Mega2560: an *enabled* and a *disabled* icon variant sharing one
slot, and image data that can live anywhere in the 256 KiB flash.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-hidden-widget-flag-done.md` | ✅ **done** | `hidden: true` widget flag — hidden widget + subtree pruned at generation time |
| 2 | `tasks/2-far-progmem-images-done.md` | ✅ **done** | baked RGB565 arrays reachable past 64 KiB via a per-screen resolver + `image_far[]` table + `image_slot` index |
| 3 | `tasks/3-async-path-behind-render-mode-done.md` | ✅ **done** | whole async path behind `JANUS_RENDER_NONBLOCKING`; scaffold-written `janus_render_config.gen.h` (mechanism A) sets it iff `non_blocking` — `blocking` `host_demo` now links for atmega2560 (`.bss` 19.2%) |
| 4 | `tasks/4-near-progmem-budget-done.md` | ✅ **done** | baked `_px` arrays emitted `JANUS_IMG_SECTION` into their own `.janus_img` flash section (linked after `.text` via `janus_img.ld` / orphan placement) — near-read fonts/descriptors/strings stay `< 0x10000`; no consumer build edit |

All four tasks are done and merged to `tasks`. Bringing up big icons on
the real board surfaced two walls — **3** (SRAM: the always-linked async
op buffer) and **4** (flash: image data displacing near-read tables) —
both now closed. `scripts/avr_gate.sh` is the concrete gate runner: it
regenerates `examples/host_demo`, links it for `-mmcu=atmega2560` in
`blocking` mode, and asserts `.bss` fits *and* every near-read symbol is
`< 0x10000`.

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
