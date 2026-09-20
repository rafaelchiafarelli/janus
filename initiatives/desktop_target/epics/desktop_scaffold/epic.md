# Epic: desktop_scaffold

The `desktop` target's Stage 8: from an `app.yaml`, `scripts/janus.sh
--target desktop` scaffolds everything a human needs to build and run a
real SDL2 window app — `main.c`, default input polls, an app
`CMakeLists.txt` — and `examples/desktop_demo` proves it on Linux. No
`.sln`/`.vcxproj`: CMake only (Visual Studio "Open Folder" works).

## Motivation

`desktop_sdl2_runtime` gave `desktop` a real display driver, but a
consumer still has to hand-write `main.c`, the poll functions and the
build. This epic scaffolds all three, once-only and human-owned after
that, the same discipline `main.c`/`janus_actions.c` already follow.

## Branch note

Branched from `main` (2026-09-20), which already contains both prior
epics merged through `epics → desktop_target → features → dev → main` —
a deliberate deviation from "epic branches from `epics`" because the
prior chain is fully merged and its integration branches are stale
relative to `dev`/`main`.

## Decisions (settled with Rafael 2026-09-20 — fixed constraints for the tasks below)

1. **Three desktop `main` templates, blocking only** —
   `main_desktop_{touch,encoder,buttons}.c.tmpl`. No async variants:
   desktop's `draw_area_async`/`display_busy` are always instant (epic
   `desktop_sdl2_runtime`, decision 5), and a `non_blocking` app.yaml
   still runs correctly on the blocking loop. Stage 8 becomes
   target-aware: `scaffold_main_c` picks the desktop template for the
   `desktop` target, the existing ones for `embedded_c`.
2. **Main loop shape:** `while (janus_desktop_driver_pump()) { ...
   same body as the embedded template for that modality ... }`, window
   sized from the app's `display.size`. The pump drains and discards SDL
   events, so input **cannot** come from the event queue — see 3.
3. **Default input polls are scaffolded once, then human-owned:**
   `src/desktop_input.c` per modality, implemented with SDL *state*
   polling (`SDL_GetMouseState`/`SDL_GetKeyboardState`) plus edge
   detection — no change to the driver or its pump. Defaults: touch =
   left mouse click at cursor; encoder = Left/Right arrows rotate ∓1,
   Enter click; buttons = Left = PREV, Right = NEXT, Enter = SELECT.
   (Mouse wheel is not state-pollable — deliberately not a default;
   the human can add it since the file is theirs.) Never overwritten
   once it exists.
4. **App `CMakeLists.txt` scaffolded once into scaffold-src**, next to
   `main.c`. Janus can't know where the generated tree lives relative
   to it, so it takes a `JANUS_GENERATED_DIR` cache variable (set by the
   human) and `file(GLOB)`s `src/*.gen.c` from it — adding a screen
   never needs a CMake edit. Links `janus_runtime` +
   `janus_desktop_driver` (from `${JANUS_GENERATED_DIR}/runtime`).
5. **`examples/desktop_demo` reuses `examples/host_demo`'s `app.yaml`
   and assets** (its own `generate.sh` points at `../host_demo/`), with
   its own `src/`. One yaml, two targets — no spec copy to drift.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-desktop-main-templates-done.md` | ✅ done | Three `main_desktop_*` templates + target-aware `scaffold_main_c`; `generate()` passes the target. |
| 2 | `tasks/2-desktop-input-scaffold-done.md` | ✅ done | Once-only `src/desktop_input.c` per modality (SDL state polling, default mappings). |
| 3 | `tasks/3-desktop-app-cmake-done.md` | ✅ done | Once-only app `CMakeLists.txt` scaffold (`JANUS_GENERATED_DIR`, glob, links runtime + driver). |
| 4 | `tasks/4-desktop-demo-done.md` | ✅ done | `examples/desktop_demo` builds via the scaffolds and runs headless; epic gate. |

Order is strict 1 → 2 → 3 → 4 (each depends on the previous).

## Acceptance gate

1. `scripts/janus.sh examples/host_demo/app.yaml --target desktop DEST
   --scaffold-src SRC` yields a tree that `cmake`-configures and builds
   with **no hand edits**.
2. The built demo starts under `SDL_VIDEODRIVER=dummy` and stays alive
   until killed/quit (smoke), and exits 0 on `SDL_QUIT`.
3. `examples/host_demo` (`embedded_c`), the Python suite, `ctest` and
   `scripts/avr_gate.sh` stay green.

## Out of scope

- Visual confirmation of the window (this sandbox has no display —
  Rafael checks on his own machine).
- Window scaling, mouse-wheel encoder, gamepad — the input file is
  human-owned; add them there.
- Android (`android` stays reserved), Windows-specific packaging.
- Any change to `janus_desktop_driver` or its pump.
