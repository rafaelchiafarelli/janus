# Epic: desktop_sdl2_runtime

The `desktop` target's Stage 4: a real SDL2 window driver, so
`scripts/janus.sh --target desktop` stops failing with "no generator
yet" and actually produces a buildable `runtime/` tree. Display only —
no input. `janus_runtime.c` and its siblings (font, layout, focus/input
dispatch — genuinely platform-agnostic C, already proven on x86 via
`host_mock`) are reused unchanged, never duplicated.

## Motivation

`janus/targets.py`'s `desktop` entry has `runtime_dir=None` since the
`multi_target_pipeline` epic — a reserved name, no generator. This epic
gives it one. Everything upstream of the driver (parse, layout, bake,
the target registry, `scripts/janus.sh --target` selection) already
works; only the driver — `draw_area_sync`/`draw_area_async`/
`display_busy` — is missing, same as it would be for any new hardware
target.

## Branch note

Branched from `multi_target_pipeline` directly, not from `epics` — this
epic's code depends on that epic's registry (`janus/targets.py`,
`_vendor_runtime`), which hasn't merged up to `epics` yet (that only
happens once every epic of `desktop_target` is done). A deliberate,
pragmatic deviation from the textbook "epics branch from `epics`" shape
for a genuinely sequential dependency, not a new precedent for
independent epics.

## Decisions (settled with Rafael 2026-09-19 — fixed constraints for the tasks below)

1. **No duplication of the fixed library core.** `runtime/embedded_c/
   {include,src,tests}` is reused as-is — it already builds on x86
   (`host_mock`'s own use proves it). A new `runtime/desktop/` holds
   *only* what's actually desktop-specific: the SDL2 driver
   (`src/janus_desktop_driver.c` + `include/janus_desktop_driver.h`) and
   its own `CMakeLists.txt`. `janus/targets.py`'s `Target` gains a
   `vendor_from: tuple[Path, ...]` (replacing the single `runtime_dir`)
   so `_vendor_runtime` can vendor `vendored_subdirs`/`vendored_root_files`
   from *multiple* source roots into one destination, in order — later
   roots' files win a name collision. `embedded_c` becomes
   `vendor_from=(runtime/embedded_c,)` (single root, behavior
   unchanged); `desktop` becomes
   `vendor_from=(runtime/embedded_c, runtime/desktop)` — the shared core
   first, then desktop's own `CMakeLists.txt` overwrites embedded_c's
   (which desktop never wants: no `janus_img.ld`, needs
   `find_package(SDL2)` instead).
2. **The vendored CMake target is still named `janus_runtime`,
   regardless of target.** A consumer's own `CMakeLists.txt`
   (`target_link_libraries(x PRIVATE janus_runtime)`) doesn't need to
   know or care which target it was generated for.
3. **RGB565 needs no pixel-format conversion.** SDL2 has a native
   `SDL_PIXELFORMAT_RGB565` — `draw_area_sync`'s `const uint16_t
   *pixels` buffer is blitted straight into a texture/surface of that
   format, no per-pixel conversion.
4. **Display-only, no input, in this epic.** `janus_desktop_driver.c`
   implements the render-side driver contract plus a small pump/
   lifecycle API (`init`/`pump`/`shutdown`) that drains the SDL2 event
   queue enough to keep the window responsive (processing `SDL_QUIT`,
   returning whether to keep running) — it does **not** interpret any
   event as touch/encoder/button input. That mapping is the scaffold's
   (next epic's) and ultimately the consumer's own job, same as
   `display_driver_stub.c` never ships an opinionated interpretation of
   real hardware signals either.
5. **`draw_area_async`/`display_busy` are real, not stubs — just always
   instant.** `draw_area_async` does exactly what `draw_area_sync` does
   and returns `true`; `display_busy` always returns `false`. This
   means a project that declares `render_mode: non_blocking` (for its
   *other*, real embedded target, sharing the same `app.yaml`) still
   builds and runs correctly for `desktop` — the async path in
   `janus_runtime.c` just completes every op in one shot. Nothing
   forbids `non_blocking` for desktop; there's just never a reason to
   choose it, which is why no `main_desktop_*_async.c.tmpl` scaffold
   exists (next epic).
6. **Headless-testable.** SDL2's dummy video driver
   (`SDL_VIDEODRIVER=dummy` env var) lets the driver's own tests run
   without a real display — required, since this sandbox (and CI, if
   this repo ever gets one) has none.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-desktop-driver-and-registry-done.md` | ✅ done | `runtime/desktop/` (SDL2 driver, its own `janus_desktop_driver` library, `CMakeLists.txt`) exists; `janus/targets.py`'s multi-root `vendor_from` lands; `desktop` registered for real. `scripts/janus.sh --target desktop` installs a real, buildable runtime. |
| 2 | `tasks/2-desktop-driver-tests.md` | not started | Host-side test coverage for the driver (headless, `SDL_VIDEODRIVER=dummy`), wired into `ctest`. |

## Acceptance gate

1. `scripts/janus.sh <app.yaml> <dest> --target desktop --scaffold-src DIR`
   succeeds and produces a `runtime/` tree that actually `cmake`-configures
   and builds, linking against real SDL2.
2. The driver's own tests pass headlessly (`SDL_VIDEODRIVER=dummy`),
   asserting `draw_area_sync` actually writes the right pixels at the
   right location and `display_busy` is always `false`.
3. `examples/host_demo` (`embedded_c`) and the full Python/`ctest` suites
   stay green — this epic must not regress the existing target.

## Out of scope

- Any input handling for desktop (touch/encoder/buttons polling) —
  `desktop_scaffold`.
- `examples/desktop_demo` itself, and the `main_desktop_*.c.tmpl`
  scaffolds — `desktop_scaffold`.
- Real, on-screen visual confirmation (an actual window rendering
  correctly to a human's eyes) — this sandbox has no display; that
  check happens on Rafael's own machine once `desktop_scaffold` lands
  and there's something to actually look at.
