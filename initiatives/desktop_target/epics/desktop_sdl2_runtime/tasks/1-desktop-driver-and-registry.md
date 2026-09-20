# Task 1: desktop-driver-and-registry

## Contract

`runtime/desktop/` exists with a real SDL2 window driver and its own
`CMakeLists.txt`; `janus/targets.py`'s `Target`/`_vendor_runtime` support
vendoring from multiple source roots into one destination; `desktop` is
registered for real. `scripts/janus.sh <app.yaml> <dest> --target desktop`
stops failing and produces a `runtime/` tree that actually configures
and builds against real SDL2.

### In

- `janus/targets.py`'s current single-root `Target(runtime_dir=...)`
  shape (`multi_target_pipeline` epic).

### Delivered

- **`janus/targets.py`**: `Target.runtime_dir: Path | None` replaced by
  `Target.vendor_from: tuple[Path, ...]` (empty tuple = reserved, not
  implemented — same meaning `runtime_dir=None` had).
  `implemented_targets()` filters on `bool(t.vendor_from)` instead of
  `t.runtime_dir is not None`. `embedded_c` becomes
  `vendor_from=(_REPO_ROOT / "runtime" / "embedded_c",)` (single root,
  identical behavior). `desktop` becomes
  `vendor_from=(_REPO_ROOT / "runtime" / "embedded_c", _REPO_ROOT / "runtime" / "desktop")`.
- **`janus/cli.py`'s `_vendor_runtime`**: loops over `target.vendor_from`
  (outer) × `target.vendored_root_files`/`vendored_subdirs` (inner),
  vendoring from each root into the same `dest_dir` in order — a later
  root's file of the same name overwrites an earlier root's copy at that
  destination path (this is how `desktop`'s own `CMakeLists.txt`
  correctly wins over `embedded_c`'s). Skip a root/subdir/file silently
  if it doesn't exist under a given root (e.g. `runtime/desktop` has no
  `tests/`, `host_mock/`, or `janus_img.ld`). `desktop`'s
  `vendored_subdirs`/`vendored_root_files` are the same tuples as
  `embedded_c`'s (`("include", "src", "tests", "host_mock")` /
  `("CMakeLists.txt",)` — no `janus_img.ld`, irrelevant off-AVR).
- **`runtime/desktop/CMakeLists.txt`** (new): defines the `janus_runtime`
  STATIC library — same name as `runtime/embedded_c/CMakeLists.txt`'s,
  so a consumer's own `target_link_libraries(x PRIVATE janus_runtime)`
  doesn't change based on target — compiling the same core sources
  (`janus_runtime.c`, `janus_input_touch.c`, `janus_input_focus.c`,
  `janus_font.c`, `janus_bound.c`, `janus_format.c` — present because
  they land in the same merged `src/` via the shared `embedded_c` root)
  plus `src/janus_desktop_driver.c`. No AVR linker-script conditional
  (genuinely N/A off-AVR). Adds `find_package(SDL2 REQUIRED)` and
  `target_link_libraries(janus_runtime PUBLIC SDL2::SDL2)`. Keeps the
  same `JANUS_RUNTIME_BUILD_TESTS` option + mock-based test executables
  `embedded_c`'s `CMakeLists.txt` defines (they still make sense here —
  they test `janus_runtime.c` itself against the scripted mock, nothing
  SDL2-specific) — task 2 adds the SDL2-driver-specific test(s) into
  this same file.
- **`runtime/desktop/include/janus_desktop_driver.h`** (new):
  ```c
  bool janus_desktop_driver_init(uint16_t w, uint16_t h, const char *title);
  bool janus_desktop_driver_pump(void);   /* false once the window should close */
  void janus_desktop_driver_shutdown(void);
  ```
- **`runtime/desktop/src/janus_desktop_driver.c`** (new): implements the
  driver contract (`draw_area_sync`/`draw_area_async`/`display_busy` —
  same signatures `runtime/embedded_c/include/janus_runtime.h` already
  declares as extern, vendored unchanged) plus the three lifecycle
  functions above.
  - `janus_desktop_driver_init`: creates an `SDL_Window` +
    `SDL_Renderer` sized `(w, h)`; creates one `SDL_Texture` of
    `SDL_PIXELFORMAT_RGB565`, `SDL_TEXTUREACCESS_STREAMING`, `(w, h)` —
    the whole panel's backing store, matching how the fixed runtime
    already treats the display as one addressable RGB565 surface.
  - `draw_area_sync`: `SDL_UpdateTexture` on the sub-rect `(x, y, w, h)`
    directly from `pixels` (already RGB565, no conversion), then
    `SDL_RenderCopy` the whole texture and `SDL_RenderPresent`. Simplest
    correct thing — no partial-present optimization, not needed at this
    panel scale.
  - `draw_area_async`: identical body to `draw_area_sync`, returns
    `true` (epic decision 5 — always instant, never actually deferred).
  - `display_busy`: always `false`.
  - `janus_desktop_driver_pump`: `SDL_PollEvent` in a loop; returns
    `false` on `SDL_QUIT`, `true` otherwise. Does not interpret any
    other event — no touch/encoder/button semantics here (epic decision
    4; `desktop_scaffold`'s job).
  - `janus_desktop_driver_shutdown`: destroys the texture/renderer/
    window.
- **Docs**: `architecture.md`'s Stage 4 section gets a short paragraph
  on the desktop driver, mirroring the existing embedded driver-contract
  writeup; the "Ownership quick reference" per-target note (added in
  `multi_target_pipeline`) updated to mention `desktop` is now populated.

### Not in scope

- Any test executable beyond what already exists via the shared
  `embedded_c` core (task 2 adds SDL2-driver-specific coverage).
- Input polling / `main_desktop_*.c.tmpl` / `examples/desktop_demo`
  (`desktop_scaffold` epic).

## Dependencies

`multi_target_pipeline` epic delivered and merged (this branch is
created directly from it, not from `epics` — see `epic.md`'s "Branch
note").

## Tests

- `tests/test_cli.py` (or a new test module): `TARGETS["desktop"].vendor_from`
  is non-empty; `implemented_targets()` now includes both `embedded_c`
  and `desktop`.
- A real `cmake -S <vendored runtime dir> -B build && cmake --build build`
  against a `scripts/janus.sh --target desktop` output succeeds and
  links (no test content yet beyond what the shared core already runs —
  task 2 adds driver-specific assertions).
- `scripts/janus.sh <fixture app.yaml> <dest> --target android` still
  fails with the documented message (`android` remains unimplemented,
  untouched by this task).

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green (`runtime/embedded_c/build`, unaffected) · a fresh
`cmake`+`build` of a `--target desktop` output succeeds and links against
real SDL2 · `scripts/avr_gate.sh` green (unaffected — `embedded_c` output
unchanged) · docs updated · this file `git mv`-ed to
`1-desktop-driver-and-registry-done.md` · commit + merge
`1-desktop-driver-and-registry → tasks`.
