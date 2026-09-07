# Epic: vu_meter

Add a new leaf widget kind, `vu` — an analog VU meter: a scale arc with
tick marks and a needle that pivots from a hub, its angle set by a
numeric bound value within an authored `range`. Same bind shape as
`progress`/`gauge`/`slider` (numeric + `range: {min, max}`), `size`
required.

## Why a new kind (not a `gauge` mode)

`gauge` takes the bar look in this initiative (`kind_visuals` task 2). A
needle-over-arc render is a genuinely different widget — different
primitives (`draw_line`, `janus_sin16`/`cos16`), different reading. It
gets its own kind + enum value, following architecture.md's "adding a
widget kind = one `draw_<kind>` + one enum value + one Python catalog
entry" rule.

## Settled decisions

- **Form:** analog needle over a **90° arc** (needle sweeps -45°..+45°
  from vertical as value goes min..max). Hub at bottom-centre of the
  geometry.
- **Colours:** `.color` = needle + ticks + hub, `.bg_color` = face fill.
  No zone colouring (green/amber/red) in v1 — a later increment can add a
  `colors:`-style band list if wanted.
- **Ticks:** 5 evenly spaced, drawn as short `draw_line` segments along
  the arc. Not authorable in v1.

## Tasks

| # | file | contract (one line) |
|---|---|---|
| 1 | `tasks/1-vu-kind-plumbing.md` | `vu` recognised end-to-end: Stage 1 (`range` required, `size` required), Stage 3b `JANUS_WIDGET_VU`, runtime enum + dispatch + a face-fill stub `draw_vu`, `Janus.md` catalog row |
| 2 | `tasks/2-vu-needle-render.md` | `draw_vu` full: face + 90° tick arc + pivoting needle at `lerp(value→[-45°,45°])` + hub disc |

## Dependency

- Task 2 needs **draw_primitives task 2** (`draw_line`, `janus_sin16`,
  `janus_cos16`) and **task 1** (`fill_circle` for the hub) merged, and
  the **draw_primitives** epic merged to `epics`.
- Task 1 has no `draw_primitives` dependency — it can land first.

## Acceptance gate

1. `.screen.yaml` with `kind: vu`, a `float`/`int` bind, `range`, and
   `size` generates; omitting `range` **or** `size` is a parse-time
   `ValueError`.
2. `test_runtime.c`: needle endpoint for `value == min` is up-left of the
   hub, for `value == max` up-right, for the midpoint ~straight up
   (assert via the mock pixel log / endpoint angle within a tolerance).
3. Python `unittest` + C `ctest` green; `scripts/avr_gate.sh` green
   (needle math is stack-only; `sin` table already in flash from
   draw_primitives).

## Out of scope

- Colour zones / redline.
- Needle ballistics / damping (snaps to the current value like every
  other display-only widget).
- A numeric readout under the needle — pair it with a `label_format`
  label instead.
- Peak-hold marker.
