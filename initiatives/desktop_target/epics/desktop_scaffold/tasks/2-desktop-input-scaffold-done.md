# Task 2: desktop-input-scaffold

## Contract

Once-only `src/desktop_input.c` per input modality, implementing the
vendor poll function(s) the desktop `main.c` (task 1) declares.

### Delivered

- `janus/templates/desktop_input_{touch,encoder,buttons}.c.tmpl`
  implementing `janus_touch_poll` / `janus_encoder_poll` /
  `janus_buttons_poll` with SDL **state** polling + edge detection
  (epic decision 3 — defaults: mouse click; Left/Right arrows + Enter;
  Left=PREV, Right=NEXT, Enter=SELECT). No event-queue use.
- `janus/stage8_scaffold/scaffold_input.py`: `scaffold_desktop_input_c(app,
  path)` — `write_if_missing`; `generate()` calls it for the `desktop`
  target only, writing `scaffold_src/desktop/desktop_input.c`.
- Tests: template per modality, once-only (hand-edited file survives),
  not written for `embedded_c`; the C compiles (`-Wall -Wextra`) against
  the vendored headers when SDL2 is available.

### Not in scope

Mouse wheel / gamepad; CMake wiring (task 3).

## Dependencies

Task 1 (declares the extern polls).

## DoD

As task 1, plus multi-target `janus.sh` scaffold lands
`desktop_input.c` under `DIR/desktop/` (shell test case).
