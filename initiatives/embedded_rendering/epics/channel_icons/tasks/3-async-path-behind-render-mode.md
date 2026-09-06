# Task 3: async-path-behind-render-mode

Status: **scoped, ready to implement** (one open decision — see "Mechanism").

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
  declarations behind the same guard.
- Whatever the **Mechanism** decision below settles: the thing that
  actually `#define`s `JANUS_RENDER_NONBLOCKING` for a `non_blocking`
  project and leaves it undefined otherwise.
- Docs: `architecture.md` Stage 4 (async section) + Stage 8, `Janus.md`
  `render_mode` paragraph — state that `blocking` now also means "links
  none of the async path".

### Mechanism — DECISION NEEDED (do not pick implicitly)

The fixed runtime has no current way to learn a per-project setting
(`render_mode` only picks a scaffold template in Stage 8 today). Options,
smallest blast radius first:

- **A.** Stage 8 / `--scaffold-src` writes `janus_render_config.gen.h`
  into the vendored `runtime/include/` — `#define
  JANUS_RENDER_NONBLOCKING` iff `non_blocking`, empty otherwise —
  and `janus_runtime.c` does `#include "janus_render_config.gen.h"`.
  Always written so the include never fails. Cost: the fixed runtime now
  includes one generated *build-config* header (not a data descriptor —
  arguably within the spirit of the split, but it *is* a new coupling).
- **B.** Stage 8 templates the vendored `runtime/embedded_c/CMakeLists.txt`
  to add `target_compile_definitions(janus_runtime PRIVATE
  JANUS_RENDER_NONBLOCKING)` when `non_blocking`. Runtime source stays
  pristine; `--scaffold-src`'s verbatim copy of `CMakeLists.txt` becomes
  a small template. Doesn't help a non-CMake consumer (PlatformIO builds
  the lib its own way).
- **C.** The consuming project passes `-DJANUS_RENDER_NONBLOCKING` in its
  own build config (documented in the scaffold README + `Janus.md`).
  One `-D`, opt-in with `non_blocking` anyway — but it's a consumer
  build-config change, the class of thing this initiative has been
  avoiding.

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
