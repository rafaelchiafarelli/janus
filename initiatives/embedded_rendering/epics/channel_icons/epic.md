# Epic: channel_icons

Make the PWM screen's full-row-height per-channel mode icons work on the
real Mega2560: an *enabled* and a *disabled* icon variant sharing one
slot, and image data that can live anywhere in the 256 KiB flash.

## Tasks

| # | file | contract (one line) |
|---|---|---|
| 1 | `tasks/1-hidden-widget-flag.md` | `hidden: true` widget flag — hidden widget + subtree pruned at generation time (no geometry, no render, no schema, no baked asset) |
| 2 | `tasks/2-far-progmem-images.md` | baked RGB565 image arrays addressed via `const __memx` (24-bit) pointers so they clear AVR's 64 KiB near-`PROGMEM` window |

The two tasks are independent in mechanism; the epic goal needs both
(task 1 lets you author two icons in one slot, task 2 lets those icons be
big).

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
