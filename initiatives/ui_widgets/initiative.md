# Initiative: ui_widgets

Close the gap between the widget catalog Janus *advertises* and what
Stage 4 actually renders. Several kinds still draw as flat solid-fill
stubs (`toggle`, `progress`, `gauge`, `led` are each just a `fill_rect`
with a state colour), a numeric-bound `label`/`header` renders **no text
at all** (only authored `text:` or a string-bound value ever draws), and
there is no meter widget. This initiative makes those render like real UI
on the OrangePi / conHMI target ([[project_janus_replaces_conhmi_rendering]]).

## Motivation

`janus_runtime.c`'s per-kind draw functions were written as a first
"real traversal, not the prototype stub" pass and never revisited:

1. `label`/`header` with a numeric `bind:` and a `text:` like
   `"Duty: %d%%"` today shows only the literal string (or, with no
   `text:`, nothing) — there is no value interpolation.
2. `draw_toggle` / `draw_checkbox` / `draw_badge` are byte-identical
   (`fill_rect` on/off) — a toggle doesn't look like a switch.
3. `draw_progress_or_gauge` is a hard two-tone split with square corners
   — no track, no rounded ends, no depth.
4. `draw_led` is a filled rectangle — not round, not shaded.
5. No VU / meter kind exists at all.

None of this needs new transport, bindings, or layout work — it's all
Stage 1 DSL recognition, Stage 3b emit, and Stage 4 draw code, plus a
small set of allocation-free drawing primitives the runtime is missing.

## Epics

- **draw_primitives** — allocation-free shape helpers in the fixed
  runtime (rounded rect, filled circle, vertical shade/lerp, integer
  line, `sin16`/`cos16` LUT), all decomposed onto the existing
  tile/`fill_rect` span model so blocking **and** async rendering keep
  working with no new async op kind. Shared substrate for the three
  epics below.
- **label_format** — printf-style specifiers in `label`/`header`:
  `text:` containing an unescaped conversion + a `bind:` interpolates the
  live bound value. Hand-rolled allocation-free formatter — no libc
  `printf` pulled in.
- **kind_visuals** — the `toggle` (switch), `progress`+`gauge` (bar), and
  `led` (shaded disc) render upgrades. Depends on **draw_primitives**.
- **vu_meter** — new `vu` widget kind: an analog needle over a scale arc.
  Depends on **draw_primitives**.

## Settled decisions (planning, 2026-09-06)

- **Format opt-in:** overload `text:` — if it holds an unescaped `%`
  conversion *and* the widget has a `bind:`, `text:` is a template.
  `%%` is a literal percent. No new DSL key.
- **Conversions:** `%d %u %ld %lld %x %s %% %f %.Nf`. `%f`/`%.Nf` via a
  minimal integer+fraction path (no libc, no libm).
- **progress/gauge:** both kinds move to the new bar look, sharing one
  draw path (gauge stops being visually distinct from progress — a real
  arc/dial gauge stays a separate future item).
- **VU form:** analog needle (scale arc + pivoting needle), not a
  segmented LED bar — this is what drives the `sin16`/`cos16` +
  `draw_line` primitives in **draw_primitives**.

## Out of scope

- Layout-engine changes (geometry, `fill`, per-kind default sizes beyond
  adding `vu` to the existing "requires explicit size" set).
- New bindings / transport / harpia-schema surface.
- Interactive (write-back) widgets — `toggle`/`slider` stay display +
  `on_press` only, exactly as today.
- A true arc/dial `gauge` distinct from `progress`.
- `image`-asset pipeline (baking, far-PROGMEM) — untouched.

## Cross-epic gate

1. `scripts/avr_gate.sh` still green — `examples/host_demo` links for
   `-mmcu=atmega2560` in `blocking` mode, `.bss` fits, no async symbol
   leaks in.
2. Python `unittest` suite and C `ctest` suite both green on host.
3. `examples/host_demo` regenerated so one screen visibly exercises: a
   `%d`/`%f` formatted label, a switch-style `toggle`, a bar-style
   `progress`, a shaded round `led`, and a `vu` needle.
