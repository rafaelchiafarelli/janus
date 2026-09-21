# Task 2: mirror-scaffold

## Contract

Stage 8 can scaffold a mirror-shaped desktop app.

### Delivered

- `janus/templates/main_desktop_mirror.c.tmpl`: `SDL_MAIN_HANDLED`,
  driver init (same window-size rule as the other desktop mains), first
  `janus_render_screen` + status/nav bars, then `while
  (janus_desktop_driver_pump())` { `mirror_link_poll(&janus_app, &state)`
  → `janus_remote_state_apply(&janus_app, &state)`; then a dirty redraw of
  the active screen (`janus_render_screen_if_dirty`) so bound-value
  updates written by the link show }; shutdown. No input polling, no
  `janus_focus_*`/`janus_handle_action`.
- `janus/templates/mirror_link.c.tmpl`: once-only `src/mirror_link.c`
  defining `bool mirror_link_poll(janus_app_t *app,
  janus_remote_state_t *out)` returning `false` ("nothing received"),
  with comments on where the transport goes and that it also writes the
  bindings struct.
- `scaffold_main_c(app, path, target, mirror=False)` /
  `render_main_c(..., mirror=False)`; `scaffold_mirror_link_c`;
  `generate(..., mirror=False)`: with `mirror` the `desktop` target gets
  the mirror main + `mirror_link.c` and **no** `desktop_input.c`;
  `embedded_c` untouched.
- `desktop_app_CMakeLists.txt.tmpl`: the input source (`desktop_input.c`
  or `mirror_link.c`) is a `@INPUT_SOURCE@` token, filled at scaffold
  time; default output byte-identical to today's.
- Python tests for all of the above (templates, once-only, generate
  wiring, no `desktop_input.c` in mirror mode, embedded_c unchanged).

### Not in scope

CLI/`janus.sh` flag (task 3).

## Dependencies

Task 1 (the header/API the templates call).

## DoD

As task 1.
