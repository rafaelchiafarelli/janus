# Initiative: desktop_target

Janus gains a second generation target — a real, windowed Windows/Linux
build of the same operator UI it already generates for constrained MCU
displays — alongside the existing embedded-C target. Scope is the
generation pipeline and a new SDL2-backed runtime; it is **not** a native
Visual Studio project (CMake only, opened via VS's own "Open Folder"
support) and it is **not** a MAVLink/serial bridge (that's
`IHMPCController`'s job, a separate hand-written app on a separate
machine's separate repo — untouched here).

## Motivation

The only way to see Janus-rendered pixels today is flashing real hardware
or reading `runtime/embedded_c/host_mock`'s logged draw-call list — there
is no way to just look at a screen while iterating on a layout. A live
window on Windows and Linux closes that loop, using the exact same
`app.yaml`/`*.screen.yaml` input Janus already parses, laid out with the
exact same Stage 1/2 pipeline — only Stage 4 (runtime) and Stage 8
(scaffolding) change per target. Scoped 2026-09-19 with Rafael; the
long-run proof is regenerating a real, independent screen spec (e.g.
ArduinoIHM's) through the new target, read-only, from that project's own
session — not part of this initiative's own acceptance gate, which stays
self-contained to this repo.

## Epics

- **multi_target_pipeline** — *(done, 2026-09-20)* restructure Janus's CLI/`generate()` to
  produce every known target (`embedded_c`, `desktop`, `android`
  reserved) in one run, nested per target internally, and teach
  `scripts/janus.sh` to install just the caller's selected target flat
  into their project — before any new target actually exists, so
  `embedded_c`'s own output stays byte-identical, only relocated.
- **desktop_sdl2_runtime** — *(done, 2026-09-20)* the new target's Stage 4: an SDL2-backed
  `draw_area_sync`/`draw_area_async`/`display_busy` window driver,
  blocking render mode only (SDL2's present is fast enough that the
  tiled/non-blocking path this exists for on SPI panels has no reason to
  exist here).
- **desktop_scaffold** — the new target's Stage 8: a CMake build (no
  `.sln`/`.vcxproj`), scaffolded `main_desktop_*.c.tmpl` per input
  modality (poll-function bodies stay human-owned — Janus wires the SDL2
  event pump's call sites, not what a keypress or click *means*), and
  `examples/desktop_demo` as the epic's — and this initiative's — proof.

Only `multi_target_pipeline` has a written `epic.md` + task files so far;
`desktop_sdl2_runtime` and `desktop_scaffold` get their own scoping pass
(decisions settled, tasks written) when work reaches them, same as every
prior multi-epic initiative in this repo.

## Cross-epic gate

`examples/desktop_demo` opens a real SDL2 window and renders on Linux;
`examples/host_demo` and the Python/`ctest` suites stay green throughout,
including after `multi_target_pipeline`'s restructuring.

## Out of scope

- A Java/Android target — a separate future initiative; only the
  `target_dir/android/` name is reserved here, unpopulated.
- Wiring any consumer project's own scripts
  (`GeneralMedicalDevices/scripts/janus-generate.sh`, whatever ArduinoIHM
  uses) to the new `scripts/janus.sh --target` interface — flagged as a
  follow-up in those repos once `desktop` actually exists, not part of
  this work.
- Anything MAVLink/serial/live-board — `IHMPCController` already owns
  that, unrelated to this initiative.
