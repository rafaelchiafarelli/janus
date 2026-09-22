# Initiative: desktop_windows_mirror

From ArduinoIHM's handoff of 2026-09-20 (`initiatives/janus_handoff/
2026-09-20-desktop-windows-and-mirror.md`, sibling repo's copy, left
untouched here): the desktop target's output was vendored into the PC
companion (IHMPCController) and built on Windows 11 / VS 2022 (MSVC) /
SDL2 2.32 (vcpkg). Two independent needs came out of that.

## Epics

- **windows_build** — *(done 2026-09-20; MSVC build itself unverified here)* the generated desktop runtime must configure, build
  and pass `ctest` under MSVC, not just GCC/Clang: MSVC-safe warning
  flags (a hard `D8021` error today), one non-portable static initializer
  in `tests/test_runtime.c`, the `SDL_main` clash on Windows (driver test
  and the desktop `main` templates), and an optional-driver switch so the
  runtime + mock tests build without SDL2. The consumer patched all of
  these by hand in its vendored copy; the next `janus-generate`
  overwrites them, so the *generator's sources* are what must change.
  Concrete fixes, no design open.
- **mirror_mode** — *(done 2026-09-20)* the desktop window mirrors what the physical board
  shows; PC input never changes the screen. The board stays the single
  source of UI truth; the PC is a viewer + command sender. Needs a
  supported, documented "apply remote state" surface (active screen,
  focus, expanded boxes, bound values) with no local side effects, and a
  way to build the desktop target without input routed to focus/activate.
  Scoped with Rafael before any task was written (remote state = screen +
  widget focus + nav focus + box bitmask; bound values stay the
  transport's; scaffold-time `janus.sh --mirror`) — see the epic.

## Cross-epic gate

The generated desktop runtime builds and its `ctest` passes under MSVC
(verified by the consumer on Windows — not reproducible in this Linux
sandbox), and `examples/desktop_demo` + `examples/host_demo` + the
Python/`ctest` suites stay green on Linux.

## Out of scope

- ArduinoIHM's transport (the board → PC MAVLink message) — that repo's.
- Android, wheel/gamepad input, anything not in the handoff.

## Status (2026-09-20)

Both epics implemented and merged to `desktop_windows_mirror`; Linux gates
green (Python suite, `ctest` on generated embedded_c + desktop trees,
`avr_gate.sh`, shell tests, `test_desktop_demo.sh`). **Open:** the
cross-epic gate's MSVC half — the consumer must confirm the generated
desktop runtime builds and passes `ctest` on Windows 11 / VS 2022. One
unverified extra: on Windows `SDL_Init` may want `SDL_SetMainReady()` when
`SDL_MAIN_HANDLED` is defined; the handoff's build passed with the define
alone, so it was deliberately not added.
