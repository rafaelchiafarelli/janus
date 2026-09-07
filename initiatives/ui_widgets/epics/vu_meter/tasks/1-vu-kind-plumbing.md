# Task 1: vu-kind-plumbing

## Contract

Recognise a new leaf widget kind `vu` end-to-end, with a face-fill stub
render. No needle yet (task 2) — this task is the mechanical
"add a kind" checklist so task 2 is a pure `draw_vu` body.

### In

```yaml
- kind: vu
  id: ch0_vu
  bind: { message: pwm, field: ch0_level, type: float }
  range: { min: 0, max: 100 }
  size: { w: 80, h: 48 }
```

### Delivered

- **`janus/stage1_parse/dsl_yaml.py`**: `"vu"` added to `_REQUIRES_RANGE`
  (so a missing `range` raises, same as `progress`).
- **`janus/stage2_layout/layout.py`**: `"vu"` added to `_LEAF_KINDS` and
  to `_REQUIRES_EXPLICIT_SIZE` (no per-kind default — the render is a
  design choice Janus can't guess, same rationale as `gauge`/`led`).
- **`janus/stage3b_embedded_c/emit_embedded_c.py`**: `_KIND_ENUM["vu"] =
  "JANUS_WIDGET_VU"`.
- **`runtime/embedded_c/include/janus_runtime.h`**: `JANUS_WIDGET_VU`
  appended to `janus_widget_kind_t` (append at the end — the enum values
  are load-bearing for `janus_font.h`'s `LARGE == 0` note only for
  `font_size`, but keep VU last to avoid renumbering existing kinds).
- **`runtime/embedded_c/src/janus_runtime.c`**:
  - `draw_vu` stub: `janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.bg_color);` (face fill only for now).
  - `render_widget`: add `JANUS_WIDGET_VU` to the bound-leaf
    dirty-check `case` list **and** to the dispatch `switch`
    (`case JANUS_WIDGET_VU: draw_vu(w, bound_struct); return;`).
  - `draw_vu` reads the bound value in task 2; for the stub it may ignore
    `bound_struct` (name the param, `(void)bound_struct;` if needed to
    avoid a warning).
- **`Janus.md`**: new `vu` row in the Leaves catalog table —
  `numeric + range: {min, max}` bind shape, "analog needle over a 90°
  arc; `size` **required**", and add `vu` to the sentence listing kinds
  that require explicit `size`.
- **`architecture.md`**: no change required (its "adding a kind" note
  already says nothing else in that doc changes) — but if the widget
  catalog is mirrored anywhere there, update it.

### Not in scope

- The needle / tick / hub drawing — task 2.
- Any `draw_primitives` dependency (stub only calls `fill_rect`).
- Input / focus (VU is display-only, not focusable — `_is_focusable`
  already returns false for any non-box leaf with no `on_press`).

## Dependencies

None. Self-contained against current code.

## Pre-work

None. Python tests use inline dict/YAML fixtures; C smoke test builds a
one-`vu`-widget descriptor by hand.

## Tests

Python:
- `kind: vu` with no `range` → `ValueError`.
- `kind: vu` with no `size` → `ValueError` (from layout).
- parse + layout of a full `vu` widget yields geometry == authored size.
- emit: descriptor initializer contains `.kind = JANUS_WIDGET_VU`.

C (`runtime/embedded_c/tests/test_runtime.c`):
- rendering a `vu` widget fills its geometry with `bg_color` and writes
  nothing outside it (stub behaviour).
- `render_widget` dispatches `JANUS_WIDGET_VU` without falling through to
  the container default.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · `Janus.md` updated ·
this file renamed `1-vu-kind-plumbing-done.md` · commit + merge
`1-vu-kind-plumbing → tasks`.
