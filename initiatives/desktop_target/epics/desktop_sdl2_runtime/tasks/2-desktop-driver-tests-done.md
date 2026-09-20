# Task 2: desktop-driver-tests

## Contract

Host-side test coverage for `runtime/desktop/src/janus_desktop_driver.c`
itself — not `janus_runtime.c`'s own logic (already covered by the
mock-based tests task 1 carried over), but whether the SDL2 driver
actually paints the right pixels at the right place. Runs headlessly
(`SDL_VIDEODRIVER=dummy`), wired into `ctest`.

### In

- Task 1's `runtime/desktop/src/janus_desktop_driver.c` +
  `include/janus_desktop_driver.h`.

### Delivered

- **`runtime/desktop/tests/test_desktop_driver.c`** (new): a small,
  self-contained test binary (mirrors `runtime/embedded_c/tests/
  test_runtime.c`'s own style — plain `assert`-based, no framework)
  covering:
  1. `janus_desktop_driver_init` succeeds under `SDL_VIDEODRIVER=dummy`.
  2. `draw_area_sync` with a known solid-color buffer, followed by
     reading back the texture's pixels (`SDL_RenderReadPixels` on the
     renderer, or lock the texture directly) — asserts the written
     region matches and nothing outside it changed.
  3. `draw_area_async` returns `true` and has the identical visible
     effect as `draw_area_sync` for the same input.
  4. `display_busy()` is always `false`, called before and after a draw.
  5. `janus_desktop_driver_pump()` returns `true` with no events
     pending, and `false` after synthesizing an `SDL_QUIT` via
     `SDL_PushEvent`.
- **`runtime/desktop/CMakeLists.txt`**: adds the
  `janus_desktop_driver_tests` executable + `add_test(...)`, gated
  behind the same `JANUS_RUNTIME_BUILD_TESTS` option task 1 carried
  over, linking `janus_runtime` (which now includes the driver) plus
  `SDL2::SDL2`.
- **Test runner note**: `SDL_VIDEODRIVER=dummy` needs to be set in the
  environment for this executable specifically (not the other,
  mock-based tests, which touch no real SDL2 state at all) — either via
  `set_tests_properties(janus_desktop_driver_tests PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy")` in the `CMakeLists.txt` (so a
  plain `ctest` run needs no special invocation) or documented if `ctest`
  itself is expected to already run under it.

### Not in scope

- Any coverage of input polling — doesn't exist yet (`desktop_scaffold`).

## Dependencies

Task 1 delivered and merged (the driver these tests exercise).

## Tests

This task's own deliverable *is* the test suite — its DoD is that suite
passing, not a separate meta-test.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green including the new `janus_desktop_driver_tests` (headless,
`SDL_VIDEODRIVER=dummy`) · `scripts/avr_gate.sh` green (unaffected) ·
docs updated if `architecture.md`'s new desktop driver paragraph
(task 1) needs a testing note · this file `git mv`-ed to
`2-desktop-driver-tests-done.md` · commit + merge
`2-desktop-driver-tests → tasks` · `tasks` merges up through
`desktop_sdl2_runtime → epics` is **not** done yet — see `epic.md`'s
acceptance gate; confirm it explicitly before that merge, and note this
epic branched from `multi_target_pipeline` directly (not `epics`), so
merging up carries that ancestry along, same as any other merge.
