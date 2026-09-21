# Task 1: remote-state-api

## Contract

`janus_remote_state_t` + `janus_remote_state_get` / `_apply` in the fixed
runtime, per epic decisions 1–3.

### Delivered

- `runtime/embedded_c/include/janus_remote.h` + `src/janus_remote.c`.
- Helpers the API needs, added where the private knowledge lives:
  `int16_t janus_focus_index(const janus_app_t *)` and
  `bool janus_focus_set_index(janus_app_t *, int16_t)` in
  `janus_input_focus.{h,c}` (the traversal order is private there);
  `void janus_box_set_expanded(const janus_widget_desc_t *, bool)` in
  `janus_runtime.{h,c}` (state only, no redraw).
- `apply`: sets the target screen's box states, then
  `janus_switch_screen` (erase + full render + status/nav bars — also for
  the same screen), then restores widget/nav focus. No-op `false` if the
  state equals the current one or is out of range. Never calls an action.
- Registered in `runtime/embedded_c/CMakeLists.txt` and
  `runtime/desktop/CMakeLists.txt` (library sources + a
  `janus_remote_tests` executable/test in each), and in
  `scripts/avr_gate.sh`'s compile list (compiles for AVR; not linked).
- `runtime/embedded_c/tests/test_remote.c`: get/apply round trip
  (screen, widget focus, nav focus, boxes), no-op on equal state, range
  rejection, apply fires no action / toggle, nav-strip and box states.
- Docs: `architecture.md` (Stage 4) — the shape, the two-side usage, the
  "bound values are the transport's" rule.

### Not in scope

Scaffolds, CLI flag (tasks 2–3).

## Dependencies

None.

## DoD

Python suite green · `ctest` on generated embedded_c and desktop trees ·
`avr_gate.sh` green · `test_janus_sh.sh` / `test_desktop_demo.sh` green ·
file renamed `-done` · epic table updated · commit + merge `→ tasks`.
