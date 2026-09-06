# Task 1: hidden-widget-flag

Status: **ready** (planning complete, not yet implemented)

## Contract

Add a static `hidden` boolean to the widget DSL. A hidden widget (and its
entire subtree) is **pruned at generation time** — it never reaches
layout, embedded-C emit, or the harpia emit.

### In

`hidden: true` on any widget node in a `.screen.yaml` (`children` or
`summary`). Default `false`. Anything not a bool → parse-time `ValueError`.

### Delivered

- `janus/ir.py`: `Widget.hidden: bool = False`.
- `janus/stage1_parse/dsl_yaml.py`:
  - `_parse_widget` reads `hidden` (default `False`).
  - `_validate_widget` rejects a non-bool `hidden`.
  - After a widget's children/summary are parsed, drop any child whose
    `hidden` is true (recursively — a hidden container takes its subtree
    with it). Pruning happens in the parser so **no other stage ever
    sees a hidden node**.
  - A hidden **root** screen widget → `ValueError` (nothing to render).
- Consequences, all falling out of "pruned in Stage 1", no extra code:
  no geometry (Stage 2 never sees it), not rendered / hit-tested /
  focusable (Stage 3b/6 never see it), its `bind` contributes nothing to
  `janus_generated.harpia` (Stage 3a never sees it), its `file:` image is
  never baked (Stage 3b never sees it).
- Docs: `Janus.md` widget catalog gets a `hidden` row/note;
  `architecture.md` Stage 1 gets a "hidden pruning" line.

### Not in scope

- No runtime `hidden` bit, no descriptor field — nothing reaches the C
  side. (A field-bound `hidden` that toggles at runtime is a separate
  future task; that one *would* need a descriptor field + runtime check +
  layout keeping a real rect.)

## Dependencies

None. Self-contained against current code.

## Pre-work

None. Test cases use inline dict/YAML fixtures, no new files.

## Tests (`tests/test_dsl_yaml*.py`)

- hidden leaf is dropped from its parent's `children`.
- hidden container drops its whole subtree.
- hidden widget in `summary` is dropped.
- non-bool `hidden` raises.
- a visible sibling authored after a hidden one lays out into the slot
  the hidden one would have taken (Stage 2 geometry unchanged vs. the
  hidden node simply not being in the file).
- end-to-end: a screen with a `hidden` `image` (with `bind` + `file:`)
  produces no extra harpia field and no baked `_px` array.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green (unchanged — no C touched) · docs updated · this file
marked done · commit + merge `1-hidden-widget-flag → tasks`.
