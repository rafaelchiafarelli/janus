# Task 2: portable-c-and-sdl-main

## Contract

Two C-level MSVC blockers fixed at the source.

### Delivered

- `runtime/embedded_c/tests/test_runtime.c`
  (`test_switch_screen_erase_covers_gaps_and_ragged_edges`): `s1_widgets`
  initialised from constant expressions (the two descriptors written
  inline / via a macro), not by copying other `const` objects (GCC
  extension; MSVC C2099). Test behavior unchanged.
- `#define SDL_MAIN_HANDLED` before `#include <SDL.h>` in
  `runtime/desktop/tests/test_desktop_driver.c` and in
  `main_desktop_{touch,encoder,buttons}.c.tmpl`; the committed
  `examples/desktop_demo/src/main.c` regenerated to match. The Python
  template tests assert the define precedes the include.

### Not in scope

Any other file — the handoff's three findings are the whole list.

## Dependencies

None (independent of task 1).

## DoD

As task 1.
