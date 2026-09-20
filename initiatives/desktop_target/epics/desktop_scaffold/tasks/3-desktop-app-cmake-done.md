# Task 3: desktop-app-cmake

## Contract

Once-only app `CMakeLists.txt` scaffolded next to `main.c`.

### Delivered

- `janus/templates/desktop_app_CMakeLists.txt.tmpl` +
  `scaffold_desktop_cmake(app, path)` (`write_if_missing`; desktop target
  only, into `scaffold_src/desktop/CMakeLists.txt`).
- Content: `JANUS_GENERATED_DIR` cache var (required — fatal error with a
  clear message if unset); `add_subdirectory(${JANUS_GENERATED_DIR}/runtime
  ...)`; `file(GLOB)` of `${JANUS_GENERATED_DIR}/src/*.gen.c` plus local
  `main.c`, `janus_actions.c`, `desktop_input.c`; include dirs for
  `${JANUS_GENERATED_DIR}/include`; links `janus_runtime` and
  `janus_desktop_driver`; `-Wall -Wextra`; runtime tests OFF.
- Tests: written once, never clobbered; the scaffolded tree configures
  and builds (needs SDL2, skipped otherwise) with only
  `-DJANUS_GENERATED_DIR=...`.

### Not in scope

Windows/VS specifics beyond plain CMake.

## Dependencies

Tasks 1–2 (the files it compiles).

## DoD

As task 1.
