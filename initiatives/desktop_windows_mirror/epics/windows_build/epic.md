# Epic: windows_build

Make the generated desktop runtime build and test under MSVC, fixing the
*generator's sources* (the consumer's hand patches are overwritten by
the next `janus-generate`).

## Branch note

Branched `main → dev → features → desktop_windows_mirror → epics →
windows_build → tasks` (dev/features/epics/tasks fast-forwarded first —
all fully merged into `main`, nothing lost).

## Decisions (fixed constraints, from the handoff + this session)

1. **One shared warning-flag variable**, `JANUS_WARN_FLAGS`, defined once
   near the top of each CMakeLists and used by every
   `target_compile_options(... PRIVATE ...)`: `/W3` under MSVC,
   `-Wall -Wextra` elsewhere. Applies to `runtime/embedded_c/CMakeLists.txt`,
   `runtime/desktop/CMakeLists.txt`, the scaffolded desktop app
   `CMakeLists.txt` template, `examples/host_demo` and
   `examples/desktop_demo/src` (the committed scaffold copy).
   `embedded_c`'s AVR cross-build is unaffected (a generator expression
   evaluates to the same GCC flags there).
2. **`SDL_MAIN_HANDLED`** is defined before `#include <SDL.h>` in
   `test_desktop_driver.c` and in the three `main_desktop_*.c.tmpl` —
   Janus's desktop `main` owns its entry point; it does not want SDL's
   `main` → `SDL_main` rewrite (nor a mandatory `SDL2::SDL2main` link).
3. **Optional driver:** `JANUS_BUILD_DESKTOP_DRIVER`, default `ON` (no
   behavior change for anyone today). When `OFF`: no
   `find_package(SDL2)`, no `janus_desktop_driver`, no driver test — the
   runtime + mock tests build without SDL2 installed.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-msvc-safe-warning-flags.md` | not started | `JANUS_WARN_FLAGS` replaces every hard-coded `-Wall -Wextra` in the runtime, the desktop app template, and both examples. |
| 2 | `tasks/2-portable-c-and-sdl-main.md` | not started | `test_runtime.c`'s non-constant static initializer fixed; `SDL_MAIN_HANDLED` in the driver test and the desktop `main` templates. |
| 3 | `tasks/3-optional-desktop-driver.md` | not started | `JANUS_BUILD_DESKTOP_DRIVER` (default ON) gates SDL2, the driver library and its test. |

## Acceptance gate

1. On Linux: every existing gate stays green (Python suite, `ctest` on a
   generated desktop tree, `avr_gate.sh`, `tests/test_janus_sh.sh`,
   `tests/test_desktop_demo.sh`), and no `-Wall -Wextra` literal remains
   in any CMakeLists or template.
2. With `-DJANUS_BUILD_DESKTOP_DRIVER=OFF` the runtime + mock tests
   configure and pass with SDL2 *not found* (simulated by hiding it).
3. **Not verifiable here:** the MSVC build itself — this sandbox has no
   MSVC/clang. The consumer confirms on Windows 11 / VS 2022; until
   then the epic's MSVC claim rests on the handoff's own findings.

## Out of scope

- Any Windows-specific packaging/CI.
- `mirror_mode` (its own epic).
