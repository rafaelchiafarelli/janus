# Task 1: desktop-main-templates

## Contract

Stage 8 scaffolds a desktop-shaped `main.c` for the `desktop` target.

### Delivered

- `janus/templates/main_desktop_{touch,encoder,buttons}.c.tmpl`: each is
  the matching embedded blocking template with (a) `display_driver_init()`
  replaced by `janus_desktop_driver_init(JANUS_DISPLAY_PANEL_W,
  JANUS_DISPLAY_PANEL_H, "<title>")` (size from the generated display
  config; title a fixed string, editable — human-owned file), (b) the
  loop as `while (janus_desktop_driver_pump()) { ... }`, (c)
  `janus_desktop_driver_shutdown()` after the loop and `return 0`. Poll
  functions are declared `extern` (defined by task 2's file).
- `scaffold_main.py`: `render_main_c(modality, render_mode, target)` /
  `scaffold_main_c(app, path, target)` — `desktop` maps to the desktop
  template regardless of `render_mode`; `embedded_c` unchanged
  (default arg keeps existing callers/tests valid).
- `janus/cli.py`: `generate()` passes `target.name` to `scaffold_main_c`.
- Python tests: template selection per (target, modality), once-only
  semantics, `embedded_c` output byte-identical to before.

### Not in scope

Input poll bodies (task 2), CMake (task 3).

## Dependencies

None beyond `desktop_sdl2_runtime` (merged).

## DoD

Contract delivered · Python suite green · `avr_gate.sh` green · docs
(`architecture.md` Stage 8) updated · file renamed `-done` · epic table
updated · commit + merge `→ tasks`.
