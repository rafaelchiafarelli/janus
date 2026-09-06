# Task 3: async-path-behind-render-mode

Status: ✅ **DONE** — contract delivered, Python + `ctest` suites green,
`scripts/avr_gate.sh` passes (`examples/host_demo` links for
`-mmcu=atmega2560` in `blocking` mode, `.bss` 1576 B / 19.2%, no async
symbol linked). Mechanism decision: **A** (see below). Merged
`3-async-path-behind-render-mode → tasks`.

## Problem

`g_async_ops[JANUS_MAX_ASYNC_OPS]` is `256 * sizeof(janus_async_op_t)` ≈
**6912 bytes** of `.bss`, and it — plus every async function — gets
linked into a project **even when `render_mode: blocking`**, because
`blit_image()` (linked whenever any `image` has a real `file:`) contains
`if (g_async_enqueue) async_enqueue_image(...)`, and `async_enqueue_image`
references `g_async_ops`. `fill_rect` and `draw_glyph` have the same
`g_async_enqueue` branch. On the 8 KiB ATmega2560 that overflow is fatal
at link (`section .bss is not within region data`). Verified with
`avr-gcc -mmcu=atmega2560` on `examples/host_demo` (which is `blocking`).

This is why no board build has ever carried a real baked image — the
pre-2026-09-06 ArduinoIHM screens used dormant `bind:`-only `image`
widgets, so `blit_image` was never linked.

## Contract

A `render_mode: blocking` project links **none** of the non-blocking
render path. A `non_blocking` project is byte-for-byte unchanged from
today.

### Delivered

- `runtime/embedded_c/src/janus_runtime.c` — `#if
  defined(JANUS_RENDER_NONBLOCKING)` around: `janus_async_op_kind_t` /
  `janus_async_op_t`; `g_async_ops` / `g_async_op_count` /
  `g_async_cursor` / `g_async_enqueue`; `async_enqueue_fill` /
  `_glyph` / `_image`; `janus_render_screen_async_start` /
  `janus_render_poll` / `janus_switch_screen_async_start`; and the
  `if (g_async_enqueue) { … }` branch inside `fill_rect`,
  `draw_glyph` (or `draw_string`), and `blit_image`. When the macro is
  absent those branches compile out entirely, so nothing references
  `g_async_ops` and the linker allocates 0 bytes for it.
- `runtime/embedded_c/include/janus_runtime.h` — the three async entry
  declarations behind the same guard, plus the `__has_include` pull of
  `janus_render_config.gen.h` (before the guard uses the macro).
- `runtime/embedded_c/CMakeLists.txt` —
  `target_compile_definitions(janus_runtime PUBLIC JANUS_RENDER_NONBLOCKING)`
  inside the `JANUS_RUNTIME_BUILD_TESTS` block.
- Mechanism A: `janus/stage3b_embedded_c/emit_embedded_c.py`
  `emit_render_config()` + `emit_files.py` `render_render_config_header()`
  + `janus/templates/render_config.h.tmpl`; `janus/cli.py` `_vendor_runtime`
  writes `janus_render_config.gen.h` into `target_dir/runtime/include/`
  (scaffold mode, both render modes).
- `scripts/avr_gate.sh` — the blocking-side gate (regenerate host_demo,
  `avr-gcc -mmcu=atmega2560 -Os` compile + link a minimal harness, assert
  `.bss` fits and `avr-nm` shows no async symbol).
- Tests: `tests/test_emit_files.py` (render-config header both ways),
  `tests/test_cli.py` + `tests/fixtures/app_with_display_non_blocking.yaml`
  (scaffold writes it into the vendored runtime, macro iff non_blocking).
- Docs: `architecture.md` Stage 4 (async section) + Stage 8, `Janus.md`
  `render_mode` paragraph — state that `blocking` now also means "links
  none of the async path".

### Mechanism — DECIDED: **A** (2026-09-06, Rafael)

Stage 8 scaffold mode (`--scaffold-src`) writes `janus_render_config.gen.h`
into the **vendored runtime's own `include/`** (`target_dir/runtime/include/`,
not the generated `include/` — that keeps it on `janus_runtime`'s existing
PUBLIC include path with zero consumer build-config change). It carries
`#define JANUS_RENDER_NONBLOCKING 1` iff `render_mode: non_blocking`, and
just a comment otherwise — written both ways so the include never dangles.

Refinement within A: `janus_runtime.h` pulls it in with
`#if defined(__has_include) && __has_include("janus_render_config.gen.h")`
rather than an unconditional `#include`, so the **in-repo** runtime (its
own `ctest`, `scripts/avr_gate.sh`, any consumer that never ran scaffold
mode) compiles fine with no such header — treated as `blocking`. The
runtime's `ctest` build forces the macro on via
`target_compile_definitions(janus_runtime PUBLIC JANUS_RENDER_NONBLOCKING)`
so `test_render_async.c` still links the path.

B/C rejected: B doesn't reach a PlatformIO consumer; C is a consumer
build-config edit, the class of thing this initiative avoids.

## Dependencies

None. Independent of task 4 (that one is a flash-addressing problem, this
is SRAM). Independent of task 2's code, though both are needed for the
epic gate.

## Pre-work

Just the Mechanism decision (planning, not code). No fixtures.

## Tests

- `test_render_async.c`'s CMake target compiles with
  `-DJANUS_RENDER_NONBLOCKING` (it exercises the async path) — unchanged
  behaviour.
- A blocking-build check: compile a TU that renders an image screen
  *without* the macro and assert (via `avr-nm` / `avr-size` in a small
  script, or `ctest` fixture) that `g_async_ops` and
  `janus_render_poll` are **absent** from the object/elf.
- `avr-gcc -mmcu=atmega2560` link of `examples/host_demo` (blocking):
  `.bss` fits the 8 KiB region. This is the concrete gate.

## DoD

Contract delivered · Python suite green · `ctest` green · the
`avr-gcc -mmcu=atmega2560` `host_demo` link fits SRAM · docs updated ·
this file marked done · commit + merge `3-async-path-behind-render-mode →
tasks`.
