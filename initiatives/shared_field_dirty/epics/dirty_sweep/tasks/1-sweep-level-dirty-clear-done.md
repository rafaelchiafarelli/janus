# Task 1: sweep-level-dirty-clear

**Status:** ready -- open questions A and B decided 2026-09-26 (see Decisions)
**Branch:** `1-sweep-level-dirty-clear` (from `tasks`)
**Depends on:** nothing

## Contract

### In

The current dirty mechanism: generated `<message>_dirty_t` (one `bool`
per field, same `offsetof` scheme as the value struct), and
`bind_consume_dirty(bind, bound_dirty)` in
`runtime/embedded_c/src/janus_runtime.c`, called from `render_widget` for
every leaf kind and reached through box `summary_children` too
(`draw_box_header`). `NULL` `bound_dirty` stays the force-draw path.

### Delivers

1. **Runtime:** during a dirty-aware sweep a widget draws if its field's
   flag is set, and **nothing clears a flag mid-sweep**. Flags that were
   set are cleared once the sweep is done. Unbound leaves keep today's rule
   ("no, skip" with a real `bound_dirty`).
2. **Native tests** (`runtime/embedded_c/tests/test_runtime.c`):
   - two leaves (e.g. `led` + `toggle`) bound to one field, in both tree
     orders: one `janus_render_screen_if_dirty` draws both, and the flag is
     clear afterwards;
   - a second sweep with nothing dirty draws neither;
   - the same through a box's `summary_children`, if a summary child can
     share a field with a body widget;
   - the existing `if_dirty` tests (around lines 290-360) still pass
     unchanged.
3. **Docs:** `architecture.md` / `Janus.md` wherever they describe the
   dirty flag ("checks and clears"), updated to "tested during the sweep,
   cleared after it", with the shared-field case named.

## Open questions -- decide before implementing (Janus principle: explicit, not inferred)

- **A. How flags get cleared.**
  - (a) **Two phases inside the runtime (recommended):** sweep with
    test-only, then clear every flag of the screen's dirty struct. That
    needs the struct's size: a generated `sizeof`, or a new
    `bound_dirty_size` in `janus_screen_desc_t`, which touches codegen.
  - (b) Record which flags were seen set during the sweep and clear only
    those. No struct size needed, but it needs scratch state.
  - (c) Give each *widget* its own dirty bit at generation time. Biggest
    change, and it alters the generated `*_dirty_t` API projects write to.
- **B. `janus_render_widget_if_dirty` on a subtree.** Clearing a flag after
  sweeping only a subtree leaves any widget outside it that shares the field
  stale again. Options: don't clear at all in the widget-level call (the
  caller owns clearing), clear anyway (document the caveat), or deprecate
  it in favour of the screen-level call.

## Decisions (Rafael, 2026-09-26)

- **A -> two passes over the tree** (a variant not in the list above: no
  codegen, no RAM scratch). `janus_render_screen_if_dirty` runs pass 1 =
  today's traversal with flags *tested only*, then pass 2 = the same
  traversal (same box-expanded rule, summaries included) clearing the flag
  of every bound leaf it reaches. A flag of a field no widget on the screen
  shows is left set (harmless).
- **B -> deprecate `janus_render_widget_if_dirty`**, keeping today's
  behaviour (per-widget test-and-clear, so the shared-field bug stays
  within a subtree call). Documented as deprecated in favour of
  `janus_render_screen_if_dirty`, with a compiler deprecation warning.

## Pre-work

None.

## Definition of done

Full test suite passes; the new tests cover both tree orders; docs
updated; task file marked done in the same commit. Downstream check (the
ArduinoIHM session, not this task): after regeneration, the PWM tab's CH0
Enabled toggle repaints on the first dirty-aware sweep.
