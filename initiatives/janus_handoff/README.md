# Handoff: janus_handoff

Not an ArduinoIHM code initiative — no task/branch ladder, nothing here
changes this repo's own code. This folder exists purely to hand two
Janus-repo items (`/home/rafael/workspace/janus`, sibling checkout) to a
session that will work in Janus directly, following the precedent
`../../handle-to-janus.md` (repo root) set on 2026-08-23 for the same
purpose. Putting it under `initiatives/` instead of another loose
root-level file: this project already accumulates more than one of these
over time (this is the second), and one growing file per handoff scales
better than one file trying to hold all of them (same "don't let
documentation sprawl" reasoning the `workflow` skill applies to task
files).

Found 2026-09-15, debugging the Janus-generated UI (`lib/GUI/`) on real
ArduinoIHM hardware (Mega2560, `COM7`/bridged over `usbipd` this session)
after a UI update pulled Janus's `main` forward 73 commits. Three symptoms
reported from the physical panel; one turned out to be ours (see bottom),
two are Janus's.

## 1. Focused box's ring doesn't erase when focus moves to another box — fixed, committed (dev)

**Repro (on this project):** `lib/GUI/bus_status.screen.yaml`'s three
boxes (`can0_box`/`can1_box`/`rs485_box`) are `collapsible: false`, no
`text:` title, no `summary:` — so each lays out with
`geometry_collapsed.h == 0` (Janus's own "box with nothing to put in a
header strip" case, `architecture.md` Stage 2's "box header" section).
Focusing one via rot1 (encoder modality) rings its *full body* per
`draw_box_header`'s documented behavior (`has_strip ? geometry_collapsed :
geometry`). Moving focus to a different box leaves the old ring on
screen — never erased.

**Root cause:** `janus_set_focus()`
(`runtime/embedded_c/src/janus_runtime.c`) redraws the previously-focused
widget via a plain `render_widget(previous, ...)`. For a headerless box
that resolves to `draw_box_header()`, which — precisely because there's no
strip — paints nothing at all when the box isn't the current focus
(`if (has_strip) { ... }` is the only painting branch; the ring-draw call
below it is gated on `box == g_focused_widget`, already false for
`previous` by the time this runs). Nothing else in the redraw path repaints
that band either: children are laid out inside the box, not necessarily
flush with its outer edge. Every *other* focusable kind avoids this
because its own normal draw already repaints the exact band the ring sits
in (a button's fill covers its whole rect; a box *with* a strip repaints
`geometry_collapsed` via its header fill) — headerless boxes are the one
case nothing does.

`janus_toggle_box()` already had the identical problem and the identical
fix (`fill_rect(lb.geometry, lb.bg_color)` before redrawing) — this port
just applies the same fix to the unfocus path.

**Fix applied** (uncommitted, working tree at `/home/rafael/workspace/janus`,
`git diff --stat`: 2 files, +51/-6):
- `runtime/embedded_c/src/janus_runtime.c` — `janus_set_focus()` now does
  `fill_rect(lp.geometry, lp.bg_color)` on the outgoing widget first, but
  *only* when it's a box with `geometry_collapsed.h == 0` (narrowly scoped
  — every other widget kind's existing redraw-on-unfocus is untouched).
- `runtime/embedded_c/tests/test_input_focus.c` — added a headerless,
  childless `box2` fixture and
  `test_unfocusing_headerless_box_clears_its_full_body_ring` (asserts a
  draw call lands at `box2`'s own `(x, y)` on unfocus — nothing else in
  the fixture could produce that draw, so it isolates the fix). Also had
  to fix `test_moving_forward_wraps_past_the_last_focusable_widget` and
  `test_moving_backward_from_first_wraps_to_last`: both hardcoded the old
  3-widget focusable count, which `box2`'s addition to the shared fixture
  changed to 4 — not a behavior regression, just the wrap math shifting by
  one stop. New test must run before
  `test_expanding_box_makes_its_child_reachable` in `main()` — that one
  permanently expands `box` (box state persists across `reset()` within a
  test binary run), which would change `box2`'s reachable position if it
  ran first.

**Verified:** `.venv/bin/python -m unittest discover -s tests` — 262
pass. Host `ctest` (`runtime/embedded_c/build_*`, freshly configured) —
11/11 suites pass, including the new one. Re-vendored into this project
(`janus-generate lib/GUI/app.yaml lib/GUI --scaffold-src lib/GUI`) and
flashed to real hardware; board boots and streams telemetry normally.
**Not yet visually confirmed on the physical panel** that the ring itself
now actually clears — I have no camera access this session, only
avrdude/serial. Worth a real look before closing this out.

**Done, Janus side (2026-09-15):** landed as `fixes/000004/headerless-box-unfocus-ring`
(`ac02f12`), merged to `dev`. Full suite re-verified before commit (11/11
`ctest`, 262/262 Python). `architecture.md`'s Stage 4 "Focus state"
section now has a line on this case, same pattern as the PROGMEM
handoff's fix being folded into Stage 4's docs. Still outstanding, on the
ArduinoIHM side: the "not yet visually confirmed on the physical panel"
point above — that verification belongs to that project, not this one.

## 2. Status bar should render above the nav strip, not below it — not built, scoped only

**What's wanted:** every screen currently renders
`[nav strip: tab cells] → [status_bar row] → [screen content]`,
top to bottom. The ask is to flip the first two: status above the nav
strip, as the true top-most band on the panel.

**Why Janus can't do this today:** `janus/stage2_layout/layout.py`,
`layout_screen()`:
```python
top = NAV_BAR_H if has_nav else 0
```
The nav band is unconditionally reserved at `y = 0`; every screen's own
content — including any authored `status_bar` row, which is just an
ordinary widget in the screen's `children` — is offset to start at
`NAV_BAR_H` and can never be pushed above it. There's no `app.yaml` knob
for this today. `build_nav_bar()` (same file) bakes the nav cells at
`y = 0` too, so both the layout offset and the baked nav geometry would
need to change together.

**Context that matters for scoping the fix:** the status_bar content in
this project (`lib/GUI/*.screen.yaml`) is, right now, *identical*
static placeholder text ("Status: TBD") authored separately in all three
screen files — nothing is bound to it. That duplication already caused a
real bug once (a copy-paste duplicate top-level `children:` key in
`relay.screen.yaml` silently dropped the whole row — YAML keeps only the
last of two identical keys, no parse error). That's a strong signal this
shouldn't stay a per-screen authored widget at all: it wants to be
app-level, Janus-owned chrome, the same relationship `nav_tabs` already
has to `app.yaml` (baked once, rendered by the fixed runtime, never
duplicated per screen).

**Suggested shape** (not committed to, just the natural extension of
what's already there — the Janus session should feel free to land on
something different):
- A new optional `app.yaml` block, e.g. `status: { text: "..." }` or a
  bound field, parsed in Stage 1 alongside `nav`/`display`/`input`.
- Stage 2's `layout_screen` reserves a second fixed-height band *above*
  the nav band when `status` is set (`top = STATUS_BAR_H + (NAV_BAR_H if
  has_nav else 0)`), and `build_nav_bar` (or a sibling) bakes the nav
  strip's own `y` at `STATUS_BAR_H` instead of `0`.
- A fixed-runtime `draw_status_bar(app)` alongside `draw_nav_bar`, called
  from the same places (`janus_switch_screen`, once at scaffold startup).
- Static text is enough for v1 — nothing here needs live-bound content
  yet (see item 3 below: the current placeholder is genuinely placeholder,
  not a stand-in for something already wired). Bindable status text is a
  reasonable follow-up once there's a real field to show, same
  "add the capability when something needs it" pattern the rest of Janus
  follows — don't build the binding path speculatively.

**For the Janus session:** this is real design work (Stage 1/2/4/8
touched, plus a new `app.yaml` field), not a bug fix — scope it as its
own small epic rather than folding it into item 1's fix. Once it exists,
this project's own three `status_bar` rows should be deleted from the
`.screen.yaml` files in favor of the new app-level field — flagging that
as a follow-up in *this* repo, not something to do as part of the Janus
work itself.

## Not a Janus bug — for context, don't rediscover this

A third symptom from the same session ("Status: TBD" visibly flickering,
~10Hz) turned out to be ArduinoIHM's own `src/main.cpp`: its ~100ms tick
called `janus_render_widget()` — the forced, unconditional redraw entry
point — on the status-bar widget every tick, regardless of whether
anything in it had changed. Since that widget is currently pure static
text with no `bind`, this redrew unchanging pixels 10x/second for no
reason. Janus's runtime behaved exactly as documented; the fix was
swapping the call site to `janus_render_widget_if_dirty()`, entirely on
this project's side. Already fixed and flashed — noted here only so it
isn't confused with the two real Janus items above if this doc gets
reused.
