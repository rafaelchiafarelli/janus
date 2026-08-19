# Janus — architecture contract

Companion to `Janus.md` (the narrative design doc) and `docs/architecture.drawio`
(the picture). This file is the reference: for every stage in the pipeline,
exactly what it receives, what it produces, who owns the output, and whether
it gets regenerated. Read `Janus.md` first for *why*; this file is *what*,
precisely enough to implement against.

Status: design only, same as `Janus.md` — nothing below is implemented yet.
Where a shape isn't nailed down, it's marked **OPEN**.

## Ownership vocabulary (used throughout)

- **Janus-owned** — regenerated in full on every run, never hand-edited,
  never partially diffed/merged. Content-diff-before-write (Janus's own
  choice, not harpia's — see `Janus.md`), but logically "full regen."
- **Human-owned** — written once by the project author, never touched by
  Janus after that. Janus may *scaffold* a starter the first time it's
  needed (open question, noted where relevant) but never overwrites it
  again.
- **Fixed library** — hand-written by Janus's own maintainers, shipped
  *with* Janus, versioned, identical across every generated project. Not
  templated per-project. Updates ship by upgrading Janus + rebuilding, not
  by regenerating.
- **External** — owned by harpia's own toolchain, out of Janus's control,
  consumed as an input/dependency.

---

## Stage 0 — Author input

**Receives:** a human, writing YAML by hand.

**Produces:**
- `app.yaml` — one per project:
  ```yaml
  screens: [device_status.screen.yaml, settings.screen.yaml]
  nav: { kind: tabs, targets: [{screen: DeviceStatus, title: "Status"}, ...] }
  ```
- `*.screen.yaml` — one per screen:
  ```yaml
  screen: <Name>
  layout: column | row
  children: [ <widget>, ... ]
  ```
  where each `<widget>` is one of the kinds in `Janus.md`'s "v1 widget
  catalog" (`label`/`header`/`button`/`image`/`progress`/`gauge`/
  `checkbox`/`radiobutton`/`led`/`column`/`row`/`box`/`radiogroup`/
  `navlist`), each with the fields documented there (`bind`, `text`,
  `asset`, `value`, `range`, `states`, `size`, `on_press`, `navigate`,
  `collapsible`, `default_expanded`, `children`).

**Owner:** human-owned, always. Janus never writes here.

---

## Stage 1 — YAML Parser (`janus/stage1_parse/dsl_yaml.py`)

**Receives:** `app.yaml` + all `*.screen.yaml` it references, from disk.

**Produces:** one in-memory `App` IR object (see "IR data contract" below),
geometry fields left empty.

**Owner:** Janus-owned (it's code, not an artifact — but listed for
completeness of the pipeline).

**Validates (parse-time errors, not silent defaults):**
- `progress`/`gauge` have `range`
- `radiobutton.value` type matches its parent `radiogroup.bind.type`
- `button.navigate` (if present) names a screen that exists in `app.yaml`
- `bind.type` is one of `string`/`int`/`int64`/`float` (harpia's real types)

Note: `size` is *not* validated here — see Stage 2, which is where the
required-vs-defaulted split actually gets enforced (it needs to know each
widget's `kind`, and defaulting is a layout-policy decision, not a parse
one).

---

## IR data contract

The shared interchange format every later stage reads or writes. Nothing
downstream touches YAML directly — this is the seam that makes JS/Node a
addable third emitter later without reworking the front half.

```python
@dataclass
class Rect:
    x: int; y: int; w: int; h: int

@dataclass
class Binding:
    message: str
    field: str
    type: Literal["string", "int", "int64", "float"]

@dataclass
class Widget:
    kind: str            # see catalog in Janus.md
    id: str
    bind: Binding | None = None
    text: str | None = None                    # static label/header/button
    asset: str | None = None                   # static image
    value: int | str | None = None             # radiobutton only
    range: tuple[float, float] | None = None   # progress/gauge only
    states: list[str] | None = None            # led only
    size: tuple[int, int] | None = None        # required on leaves
    on_press: str | None = None                # button only, opaque action name
    navigate: str | None = None                # button only, screen name
    collapsible: bool = False                  # box only
    default_expanded: bool = True              # box only
    layout: Literal["column", "row"] | None = None   # containers only
    children: list["Widget"] = field(default_factory=list)
    # --- filled in by the layout pass, empty after parsing ---
    geometry: Rect | None = None
    geometry_collapsed: Rect | None = None     # box only

@dataclass
class Screen:
    name: str
    root: Widget          # top-level container for this screen

@dataclass
class NavTarget:
    screen: str
    title: str

@dataclass
class App:
    screens: list[Screen]
    nav: list[NavTarget] | None    # kind is always "tabs" in v1
```

---

## Stage 2 — Layout pass (`janus/stage2_layout/layout.py`)

**Receives:** `App` IR, `geometry`/`geometry_collapsed` all `None`.

**Produces:** the *same* `App` object, mutated in place — every `Widget`
now has `geometry` filled with an absolute `Rect`; every `box` additionally
gets `geometry_collapsed` (a second, independent rect set for its
non-expanded state).

**Owner:** Janus-owned. Pure function — no I/O, deterministic given the
same IR, so it's safe to unit-test directly against expected `Rect` output.

**Rule:** a container's direction (`column` stacks vertically, `row`
stacks horizontally) plus each child's size determines every child's
absolute position; a container's own size is derived bottom-up from its
children (sum along the stack direction, max across it) plus a fixed
inter-sibling gap — no container is ever explicitly sized by the author.
No widget ever authors its own position.

**Size resolution per leaf** (implements the split in `Janus.md`'s v1
widget catalog):
- `progress`, `gauge`, `image`, `led` — `size` is **required**; the layout
  pass raises if missing (their dimensions are a real drawing choice, not
  derivable).
- `label`, `header`, `button`, `checkbox`, `radiobutton` — `size` is
  optional; if omitted, a fixed per-kind v1 placeholder default is used
  (e.g. `label` → 60×12, `button` → 64×20, `checkbox`/`radiobutton` →
  12×12). Authors can still override with an explicit `size`.

**`box` header:** `box` has no dedicated title field — it reuses the
generic `Widget.text` field already shared by `label`/`header`/`button`.
Its content area is offset below a fixed-height header strip
(`BOX_HEADER_H`, a layout constant, not part of the IR contract); `box`'s
`geometry_collapsed` covers the header strip only, `geometry` covers
header + body.

---

## Stage 3a — harpia Include emitter (`janus/stage3a_harpia/emit_harpia.py`)

**Receives:** `App` IR (post-layout; only reads `bind`, ignores geometry).

**Produces:** `janus_generated.harpia` — one `message` block per distinct
`Binding.message` name across every screen in the app, fields deduped by
`field` name if two widgets bind the same field:

```
message device{
    string name;
    int battery_level;
};
```

**Owner:** Janus-owned, full regen every run, content-diffed before write.

**Rule:** flat only — no sub-messages emitted in v1 (`Janus.md`'s "flat for
now" decision). `Binding.type` maps 1:1 to harpia's `int`/`int64`/`float`/
`string` — no inference, no widening.

---

## Stage 3b — embedded-C data emitter (`janus/stage3b_embedded_c/emit_embedded_c.py`, `emit_files.py`)

**Receives:** `App` IR (post-layout; geometry required).

**Produces**, per project:
- `{screen}_screen.gen.c` / `.gen.h` per screen — the widget descriptor
  array + `janus_screen_desc_t` (see "Runtime library" below for the
  struct shapes these initialize). Implemented: `emit_screen(screen,
  screen_index_by_name)` — the index map (`screen_index_map(app)`) is only
  required if the screen has a `navigate` button; it resolves
  `.navigate_target` to the target screen's index and is what actually
  enforces the "navigate target must exist" rule Stage 1 defers.
- `janus_actions.gen.h` — one shared file: an enum with one value per
  distinct `on_press` string across *all* screens in the app (deduped).
  `navigate` values do **not** get an enum entry here — see Stage 5.
  Implemented: `emit_actions_header(app)`.
- `janus_app.gen.c` — the `janus_app_t` table: an array of pointers to
  every screen's `janus_screen_desc_t`, plus `nav_titles` (parallel array,
  `NULL` if `app.nav` is unset). Implemented: `emit_app_table(app)`;
  raises if `app.nav` doesn't cover every screen.

**`.bound_struct` resolution (v1: one message per screen).** Each
widget's `.bind.field_offset = offsetof({message}_t, {field})` already
assumes `{message}_t` is a real, visible C struct type — that's Stage 7's
long-standing open question (does harpia's `ZmqAdapter` emit anything
usable as a plain C struct?), still unresolved. `.bound_struct` needs an
*instance* of that type, though, and something has to give for Stage 4 to
read live values at all: `emit_screen()` collects the distinct
`Binding.message` names used by that screen's widgets
(`screen_bound_messages`) and requires there be at most one. With one,
it emits `.bound_struct = &{message}_instance` and
`render_screen_source` adds a conditional `#include "janus_bindings.h"`
(built in Python, same pattern as `emit_app_table`'s `titles_ref`, no new
templating logic) — a new fixed-name convention, human/vendor-owned
(never regenerated), analogous to `src/display_driver.c`. With zero
bindings, `.bound_struct = NULL` and the include is omitted. With more
than one distinct message, `emit_screen()` raises `ValueError` — matches
Stage 1's "validate, don't silently default" philosophy. This resolves
enough of Stage 7's question for a single-message screen to compile and
read live values; it does **not** resolve whether harpia's `ZmqAdapter`
can actually produce a `janus_bindings.h`-shaped output, or what happens
past one message per screen — both stay open.

**Implementation status:** implemented and tested (75 tests): the raw
declarations (`emit_screen`/`emit_actions_header`/`emit_app_table`), the
`.tmpl` wrapper turning them into complete standalone files with
`#include`s and header guards (`emit_files.py`, `janus/templates/*.tmpl`),
and `generate.write_project` writing them to disk with
content-diff-before-write.

**Owner:** Janus-owned, full regen every run, content-diffed before write
(this is the deliberate improvement over harpia's own unconditional-write
behavior — see `Janus.md` — because these files get compiled, and an
untouched-content rewrite would still force a recompile via mtime).

**Mechanism:** harpia-style `.tmpl` + `str.format()`, e.g.:

```c
// screen.c.tmpl
#include "{screen_var}_screen.gen.h"
{extra_includes}
{body}
```

where `{extra_includes}` is a Python-built, possibly-empty string —
`#include "janus_actions.gen.h"` when the screen has any `on_press`
widget, `#include "janus_bindings.h"` when it has any `bind` — and
`{body}` is the widget-array + `janus_screen_desc_t` initializer text
built by `emit_screen`, one initializer line per widget, exactly like
`harpia/Database/CrudlAdapter.py`'s `_create_locals`.

---

## Stage 4 — Runtime library (`runtime/embedded_c/`)

**Receives:** nothing generated. Hand-written once, shipped with Janus,
vendored into every generated project (copy or build-system dependency —
**OPEN**, not yet decided which).

**Produces — the stable C API every generated file and every human file
compiles against** (`runtime/embedded_c/include/janus_runtime.h`, which
pulls in `<stddef.h>` itself since every generated widget initializer
uses `offsetof()`/`NULL` — found compiling the first real generated
project against this header):

```c
typedef enum {
    JANUS_WIDGET_LABEL, JANUS_WIDGET_HEADER, JANUS_WIDGET_BUTTON,
    JANUS_WIDGET_IMAGE, JANUS_WIDGET_PROGRESS, JANUS_WIDGET_GAUGE,
    JANUS_WIDGET_CHECKBOX, JANUS_WIDGET_RADIOBUTTON, JANUS_WIDGET_RADIOGROUP,
    JANUS_WIDGET_LED, JANUS_WIDGET_BOX, JANUS_WIDGET_COLUMN, JANUS_WIDGET_ROW,
} janus_widget_kind_t;

typedef enum { JANUS_FIELD_NONE, JANUS_FIELD_INT, JANUS_FIELD_INT64, JANUS_FIELD_FLOAT, JANUS_FIELD_STRING } janus_field_type_t;

typedef struct { int16_t x, y, w, h; } janus_rect_t;

typedef struct {
    uint16_t field_offset;         /* offsetof() into bound_struct; 0 if unbound */
    janus_field_type_t field_type;
    float range_min, range_max;    /* progress/gauge only */
} janus_bind_t;

typedef int16_t janus_action_id_t;   /* see note below — not janus_action_t */

typedef struct janus_widget_desc {
    janus_widget_kind_t kind;
    const char *id;
    janus_rect_t geometry;             /* also the "expanded" rect for box */
    janus_rect_t geometry_collapsed;   /* box only, ignored otherwise */
    bool initial_expanded;             /* box only, ignored otherwise — baked from Widget.default_expanded */
    janus_bind_t bind;
    janus_action_id_t action;          /* on_press only; 0 otherwise (== JANUS_ACTION_NONE by convention) */
    int16_t navigate_target;           /* navigate only; index into janus_app_t.screens, -1 otherwise */
    uint8_t focus_order;               /* encoder/button traversal — reserved, unused; touch doesn't need it */
    const struct janus_widget_desc *children;
    uint16_t child_count;
} janus_widget_desc_t;

typedef struct {
    const char *name;
    const janus_widget_desc_t *widgets;
    uint16_t widget_count;
    const void *bound_struct;   /* e.g. &device_t instance — see open question below */
} janus_screen_desc_t;

typedef struct {
    const janus_screen_desc_t *const *screens;
    const char *const *nav_titles;   /* parallel to screens; NULL if app.nav is unset (no tab bar) */
    uint16_t screen_count;
    uint16_t active_screen;          /* the one piece of app-level runtime state */
} janus_app_t;

/* driver contract, carried forward from the deleted Copilot branch's DESIGN.md */
void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels);
bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels);
bool display_busy(void);

/* runtime entry points */
void janus_render_screen(const janus_screen_desc_t *screen);
void janus_switch_screen(janus_app_t *app, uint16_t screen_index);   /* used by navigate */
void janus_toggle_box(const janus_widget_desc_t *box);               /* re-renders just that subtree */
bool janus_box_is_expanded(const janus_widget_desc_t *box);          /* reads the state above; Stage 6 hit-testing needs it */
```

**Why `janus_action_id_t`, not `janus_action_t`, on the descriptor.**
`janus_action_t` is defined by the *generated*, per-project
`janus_actions.gen.h` (Stage 3b) — but `janus_widget_desc_t` is defined
once, in this fixed header, with no guarantee any per-project file has
been `#include`d first. Typing the field as a generic `int16_t` id
(enum constants convert to it implicitly, no cast needed at the
generation site) keeps the fixed library fully independent of any
generated enum. Stage 6, when it lands, casts back to `janus_action_t`
right before calling `janus_handle_action`.

**Box collapse state.** `janus_widget_desc_t` instances are
`static const` arrays baked at generation time — there's nowhere in them
to store a *mutable* "currently expanded" bit. `janus_runtime.c` keeps a
small fixed-capacity table mapping descriptor pointer → current expanded
bit (descriptors have program-lifetime-stable addresses, so pointer
identity is a safe key), seeded from `.initial_expanded` the first time
a box is rendered and flipped by `janus_toggle_box`.

`janus_runtime.c` implements traversal + tiling + one internal
`draw_<kind>()` per widget kind, dispatched by `kind` — this is where
DESIGN.md's actual rendering logic lives, finally for real (not the
deleted Copilot stub). Scope note: only the synchronous path
(`draw_area_sync`) is driven by the traversal so far — `draw_area_async`/
`display_busy` are declared (any driver must still provide them) but not
yet called by anything; polled/non-blocking scheduling is real future
work. Leaf rendering is a kind-distinct solid fill of `geometry` (no
font/glyph engine exists yet — still "Stage 2+" in `Janus.md`), except
`progress`/`gauge` (fraction of range) and `checkbox` (checked/unchecked)
which read the *live* bound value and vary the fill accordingly — the
concrete difference from the deleted Copilot stub's "empty buffer
regardless of screen contents."

**Owner:** fixed library. **Adding a widget kind = adding one
`draw_<kind>()` function here + one enum value + one entry in the Python
kind catalog — nothing else in this document changes.**

---

## Stage 5 — Action dispatch

**Receives:** `janus_actions.gen.h` (Stage 3b's enum).

**Produces:** `src/janus_actions.c` — human-owned:

```c
void janus_handle_action(janus_action_t action) {
    switch (action) {
        case JANUS_ACTION_REBOOT:            /* ... */ break;
        case JANUS_ACTION_FACTORY_RESET:     /* ... */ break;
        case JANUS_ACTION_OPEN_ABOUT_DIALOG: /* ... */ break;
        default: break;
    }
}
```

**Owner:** human-owned, one `case` added per new `on_press` value, never
regenerated. Resolved: Janus scaffolds this file with `TODO` cases the
*first* time it's needed (`janus/stage5_actions/scaffold_actions.py`,
`scaffold_actions_c`) — once it exists, Janus never touches it again.

**Rule — the two button verbs are not the same path:** `on_press` values
route here, through the human's own switch. `navigate` values never reach
this file — they're handled directly by `janus_switch_screen()` inside the
fixed runtime library, because Janus itself understands "go to this
screen" (it validated the target at parse time); it does not understand
what "reboot" means.

---

## Stage 6 — Input dispatch (touch implemented; encoder/buttons still deferred)

**Receives:** a touch point (`x, y`) + read-only access to the active
screen's `janus_widget_desc_t[]` (for `geometry`, hit-testing). Encoder
deltas / GPIO edges and `focus_order`-based traversal stay deferred —
the field exists in the struct but nothing populates or reads it yet.

**Produces:** a `janus_input_result_t` — `{kind, widget, action,
navigate_target}` where `kind` is `JANUS_INPUT_NONE` / `_ACTION` /
`_NAVIGATE` / `_TOGGLE_BOX`. **Not** a `(widget, janus_action_t)` pair as
first sketched: the fixed library can't type against the generated,
per-project `janus_action_t` any more than Stage 4's descriptor could
(same reason — see `janus_action_id_t` there). `action` is a
`janus_action_id_t`; the caller casts it to `janus_action_t` right
before calling `janus_handle_action`, same split Stage 5 already
established between what the fixed library understands autonomously
(`navigate`, box toggling) and what only the human's file understands
(`on_press`).

**Hit-testing** (`janus_touch_hit_test`, implemented in
`janus_input_touch.c`): point-in-rect against the already-baked absolute
geometry, deepest match wins. `box`'s `geometry_collapsed` doubles as its
header hit-region — `layout.py` already sets it to exactly the header
strip's bounds regardless of current expand state, so a point inside it
is always a header tap (→ `TOGGLE_BOX`), with no separate header-height
constant needed at runtime. A point elsewhere in `geometry` only recurses
into a box's children when `janus_box_is_expanded(box)` is true — a
collapsed box's children are never hit-testable, matching Stage 4's own
render-time skip. `column`/`row`/`radiogroup` have no rect of their own
action-wise and just recurse. A leaf hit with neither `navigate_target`
nor `action` set (a plain label, an unwired checkbox) is a deliberate,
defined miss (`JANUS_INPUT_NONE`) — nothing to dispatch. If a widget
somehow has both `navigate` and `on_press` set, `navigate` wins (an
arbitrary but documented tie-break; nothing currently prevents authoring
both).

**Touch driver contract** (`janus_input_touch.h`, parallel to the output
driver contract): `bool janus_touch_poll(int16_t *x, int16_t *y)` —
vendor/host-provided, non-blocking, mirrors `display_busy()`'s polling
style. Returns true and fills `x`/`y` once per new touch.

**Owner:** fixed library, one module per modality — `janus_input_touch.h`
/ `.c` exist; `janus_input_encoder.c` / `janus_input_buttons.c` don't yet.
Landed without touching any earlier stage except `main.c.tmpl`'s scaffold
(Stage 8), which now actually calls into this instead of a bare TODO.

---

## Stage 7 — harpia's downstream codegen (external)

**Receives:** the full assembled schema — human root `.harpia` (which
`import`s Stage 3a's `janus_generated.harpia`) — via harpia's own `harpia`
CLI, specifically `ZmqAdapter` for this target (REST/gRPC are out per
`Janus.md`'s "Targets" section).

**Produces:** transport + (de)serialization code for every bound message.

**Owner:** external — entirely harpia's own toolchain, regenerated by
running `harpia`, not Janus.

**RESOLVED (research pass, 2026-08-19), and it's a "no":** read
`harpia/ZmqAdapter/ZmqAdapter.py` and its golden output directly
(`harpia/tests/golden/zmq/users_*_zmq.h`). `ZmqAdapter` emits a
header-only **C++** class per transport role (`<name>_sender`,
`<name>_receiver`, `<name>_publisher`, `<name>_subscriber`) —
`std::string`, `zmq::socket_t&`, C++ namespaces/classes/constructors
throughout — wrapping a protobuf-generated **C++ class**
(`::<name>` from `protofiles/<name>_<hash>.pb.h`, standard
`protoc --cpp_out` output: getters/setters, no public data layout). There
is no plain C struct anywhere in this path, for any message, and there
structurally can't be one: this is protobuf's own C++ code generator, the
same one every other harpia backend builds on. This isn't a temporary gap
— it's the target mismatch `Janus.md`'s "Why this isn't part of the
harpia repo" section already named ("harpia's generated C++ output
assumes protobuf... none of which fit an 8-bit-class, ~2 KiB-RAM embedded
target"), now confirmed concretely at the struct-layout level rather than
argued in the abstract.

**Consequence for Stage 3b/4/6:** Janus can never point
`.bound_struct`/`.field_offset = offsetof(...)` at anything harpia emits.
The `janus_bindings.h` convention `examples/host_demo` hand-writes today
(Stage 3b's `.bound_struct` resolution) was pointing the right direction
but for the wrong reason — it shouldn't be a permanent human/vendor
hand-write standing in for a harpia output that will never arrive; it
should become **another Janus-generated artifact**, alongside
`janus_generated.harpia`: a plain C struct per bound message, generated
directly from the same `Binding`s `emit_harpia.py` already walks (same
field names/types, independently emitted in two languages for two
targets — harpia's C++/protobuf schema for desktop/server, Janus's own
plain C struct for embedded — not one consuming the other's output).
Not yet implemented; this is the natural next Stage 3b increment once
picked up.

**Bonus finding, corrects `Janus.md`:** that doc's "Origin"/"Salvageable"
sections state the Copilot `GuiAdapter/` branch was "deleted, both
locally and on `origin`". It is not — `harpia`'s `dev` branch (current
`HEAD`, `git ls-files GuiAdapter/`) still has it in full
(`DESIGN.md`/`README.md`/`runtime/`/`tool/`), unchanged since its
original commit. Doesn't change any decision already made here (Janus
staying a standalone repo, the DESIGN.md salvage plan, the "why not part
of harpia" reasoning all stand on their own merits either way), but
`Janus.md`'s deletion claim itself is factually wrong and worth fixing
next time that file's touched, so a future session doesn't rely on
"memory of what was in it" when the actual code is one `cd` away.

---

## Stage 8 — Firmware assembly

This is the "how does it all actually get compiled" question.

**Compilation units and who owns each:**

| file | owner | regenerated? |
|---|---|---|
| `runtime/embedded_c/src/janus_runtime.c` | Janus (fixed library) | no — only on Janus version upgrade |
| `build/generated/{screen}_screen.gen.c` | Janus | yes, every build |
| `build/generated/janus_actions.gen.h` | Janus | yes, every build (cheap — just names) |
| `build/generated/janus_app.gen.c` | Janus | yes, every build |
| `janus_generated.harpia` → harpia's own codegen output | harpia (external) | yes, via `harpia` CLI |
| `src/janus_actions.c` | human | no |
| `src/main.c` | human (Janus-scaffolded once, `scaffold_main_c`) | no |
| `src/display_driver.c` | human/vendor | no |

**Runtime call flow, boot to first render:**
1. `main()` calls the vendor's `display_driver_init()`.
2. `main()` constructs `janus_app_t app` from `janus_app.gen.c`'s table,
   `active_screen = 0`.
3. `main()` calls `janus_render_screen(app.screens[app.active_screen])`,
   which walks that screen's `janus_widget_desc_t[]`; for each widget it
   dispatches to the matching internal `draw_<kind>()`, which reads the
   live value at `bind.field_offset` inside `bound_struct` and calls the
   vendor driver's `draw_area_sync`/`draw_area_async` for that widget's
   `geometry` rect.

**Runtime call flow, on interaction:**
4. `main()`'s event loop (`main.c.tmpl`, Stage 8) polls
   `janus_touch_poll(&x, &y)` and, on a new touch, calls
   `janus_touch_hit_test(screen, x, y)` (Stage 6).
5. The scaffolded `main.c` switches on the result's `kind` — this is the
   one place all three outcomes meet, and it's the human's file (editable
   after scaffolding) that does the switching, not the fixed library:
   - `JANUS_INPUT_ACTION`: `janus_handle_action((janus_action_t)hit.action)`
     — the human's file, Stage 5.
   - `JANUS_INPUT_NAVIGATE`: `janus_switch_screen(&app, hit.navigate_target)`
     — the fixed library sets `active_screen` and re-renders the *new*
     screen from *its* geometry table. Only one screen's widgets are ever
     live, matching the ~2 KiB transient-buffer budget.
   - `JANUS_INPUT_TOGGLE_BOX`: `janus_toggle_box(hit.widget)` — flips
     that widget's local expand bit and re-renders just that subtree,
     using `geometry` vs `geometry_collapsed` — a tile-scoped redraw, not
     a full screen repaint, per DESIGN.md's tiling model.

---

## Ownership quick reference

| artifact | owner | regenerated? |
|---|---|---|
| `app.yaml`, `*.screen.yaml` | human | — (source of truth) |
| `janus_generated.harpia` | Janus | every run |
| human root `.harpia` | human | never |
| `{screen}_screen.gen.c/.h` | Janus | every run |
| `janus_actions.gen.h` | Janus | every run |
| `janus_app.gen.c` | Janus | every run |
| `runtime/embedded_c/*` | Janus (fixed library) | on Janus upgrade only |
| `src/janus_actions.c` | human | never |
| `src/main.c` | human | never |
| `src/display_driver.c` | human/vendor | never |
| harpia's `ZmqAdapter` output | harpia (external) | every `harpia` run |
