# Epic: mirror_mode

The desktop window mirrors what the physical board shows. PC keyboard/
mouse never change the screen; the board is the single source of UI
truth, the PC a viewer + command sender. From ArduinoIHM's handoff
(2026-09-20, item 2), which asked Janus to define the "remote state"
shape so the transport can serialize exactly that.

## Branch note

`main → dev → features → desktop_windows_mirror → epics → mirror_mode →
tasks` (`epics` already carries the merged `windows_build` epic).

## Decisions (settled with Rafael 2026-09-20 — fixed constraints for the tasks below)

1. **Remote state is small and UI-only:**
   ```c
   typedef struct {
       uint16_t screen;          /* app->active_screen */
       int16_t  focus;           /* index among the active screen's reachable
                                    focusable widgets, in janus_focus_move's
                                    traversal order; -1 = none */
       int16_t  nav_focus;       /* previewed nav-strip tab; -1 = none */
       uint16_t boxes_expanded;  /* bit i = i-th box of the active screen's
                                    widget tree (depth-first, tree order,
                                    independent of expansion) is expanded */
   } janus_remote_state_t;
   ```
   Two focus fields because the runtime itself keeps two focus states
   (widget vs. nav strip); a bit per box because box expansion is UI
   state the board owns too (indexed by tree position, since the
   runtime's own box table is keyed by pointer in first-use order —
   not stable across devices). Capacity 16 boxes = `JANUS_MAX_BOXES`.
2. **Bound values are NOT in the struct.** They live in the app's
   generated `janus_bindings.gen.h` struct, which the transport already
   has to serialize; the PC writes that struct, then the mirror `main`
   redraws it with `janus_render_screen_if_dirty`. Apply never reads or
   writes bound values.
3. **API (fixed runtime, own translation unit `janus_remote.c`):**
   `void janus_remote_state_get(const janus_app_t *, janus_remote_state_t *)`
   (board side) and `bool janus_remote_state_apply(janus_app_t *, const
   janus_remote_state_t *)` (PC side) — a full erase + redraw of exactly
   that state (a pure function of it, so mirrors are pixel-comparable),
   firing **no** actions/navigation/toggles; returns `false` and draws
   nothing when the state already matches (a host applying every
   telemetry tick must not flicker) or when `screen`/indices are out of
   range. Own translation unit → an app that never calls it links none
   of it (zero AVR flash).
4. **Mirror mode is a scaffold-time choice: `janus.sh --mirror`.** It
   only changes what the `desktop` target scaffolds — a mirror `main.c`
   (no input polling; loop = pump + `mirror_link_poll` + apply + dirty
   redraw) and a stub `src/mirror_link.c` (the transport hook, returns
   "nothing received") **instead of** `desktop_input.c`. The app.yaml is
   unchanged and shared with the board. `--mirror` without a `desktop`
   target is a usage error (never a silent no-op).
5. Not in scope: the transport itself (ArduinoIHM's MAVLink message),
   serializing bound values, Android.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-remote-state-api-done.md` | ✅ done | `janus_remote.{h,c}` (`get`/`apply`) + the two small helpers it needs (`janus_focus_index`/`janus_focus_set_index`, `janus_box_set_expanded`); C tests. |
| 2 | `tasks/2-mirror-scaffold-done.md` | ✅ done | Mirror `main_desktop_mirror.c.tmpl` + `mirror_link.c` stub; `generate(mirror=)`; CMake scaffold lists the right input source. |
| 3 | `tasks/3-janus-sh-mirror-flag-done.md` | ✅ done | `janus.sh --mirror` (+ `janus.cli --mirror`), validation, shell test that builds and runs a mirror app headless. |

## Acceptance gate

1. Board-side `get` → PC-side `apply` round-trips every field on a
   fixture with nav, boxes and focus, and `apply` fires no action
   (C tests).
2. `janus.sh app.yaml --target desktop DIR --scaffold-src SRC --mirror`
   configures and builds with only `-DJANUS_GENERATED_DIR`, and runs
   headless until quit; no `desktop_input.c` is scaffolded.
3. Every existing gate stays green (Python suite, `ctest` on generated
   embedded_c + desktop trees, `avr_gate.sh`, shell tests).

## Out of scope

See decision 5.
