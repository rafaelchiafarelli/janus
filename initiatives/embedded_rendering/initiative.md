# Initiative: embedded_rendering

Runtime/codegen capabilities the embedded-C target needs that only surface
on real constrained hardware — the ArduinoIHM Mega2560 board being the
first consumer. Scope is the render path and its generation, not new
widget kinds or transport.

## Motivation

Bringing the Janus-generated PWM screen up on the real board with
full-row-height per-channel mode icons hit two hard limits at once:

1. There was no way to declare "this widget is not shown" — needed so an
   *enabled* and a *disabled* icon variant can occupy the same slot and
   one be chosen.
2. Baked RGB565 image arrays (~90 KiB for four row-height icons) overflow
   the classic-AVR 64 KiB near-`PROGMEM` addressing window: `avr-as`
   fails with `value of NNNNN too large for field of 2 bytes` on the
   pixel-array pointer relocation, and even past that `memcpy_P` can't
   reach the data.

## Epics

- **channel_icons** — both of the above, scoped tightly around making the
  PWM channel icons work on the board.

## Cross-epic gate

`avr-gcc -mmcu=atmega2560` compiles the `examples/host_demo` generated
output + vendored runtime with row-height icons and no near-`PROGMEM`
overflow; the Python suite and the C `ctest` suite stay green on host.
