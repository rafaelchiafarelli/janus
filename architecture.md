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

**Implementation status:** the three functions above emit correct,
brace-balanced C text today (36 tests). Still deferred: the `.tmpl`
wrapper that turns this into complete, standalone files with `#include`s
and header guards — everything so far is just the declarations/
initializers themselves, verified via substring assertions, not yet
written to disk as real `.c`/`.h` files.

**Owner:** Janus-owned, full regen every run, content-diffed before write
(this is the deliberate improvement over harpia's own unconditional-write
behavior — see `Janus.md` — because these files get compiled, and an
untouched-content rewrite would still force a recompile via mtime).

**Mechanism:** harpia-style `.tmpl` + `str.format()`, e.g.:

```c
// screen_table.c.tmpl
#include "janus_runtime.h"

static const janus_widget_desc_t {screen_var}_widgets[] = {{
{widget_entries}
}};

const janus_screen_desc_t {screen_var}_screen = {{
    .name = "{screen_name}",
    .widgets = {screen_var}_widgets,
    .widget_count = {widget_count},
    .bound_struct = &{bound_struct_instance},
}};
```

`{widget_entries}` is a Python-built string, one initializer line per
widget, exactly like `harpia/Database/CrudlAdapter.py`'s `_create_locals`.

---

## Stage 4 — Runtime library (`runtime/embedded_c/`)

**Receives:** nothing generated. Hand-written once, shipped with Janus,
vendored into every generated project (copy or build-system dependency —
**OPEN**, not yet decided which).

**Produces — the stable C API every generated file and every human file
compiles against** (`runtime/embedded_c/include/janus_runtime.h`):

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

typedef struct janus_widget_desc {
    janus_widget_kind_t kind;
    const char *id;
    janus_rect_t geometry;             /* also the "expanded" rect for box */
    janus_rect_t geometry_collapsed;   /* box only, ignored otherwise */
    janus_bind_t bind;
    janus_action_t action;             /* on_press only; JANUS_ACTION_NONE otherwise */
    int16_t navigate_target;           /* navigate only; index into janus_app_t.screens, -1 otherwise */
    uint8_t focus_order;               /* input dispatch — reserved, unused until Stage 6 lands */
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
```

`janus_runtime.c` implements traversal + tiling + one internal
`draw_<kind>()` per widget kind, dispatched by `kind` — this is where
DESIGN.md's actual rendering logic lives, finally for real (not the
deleted Copilot stub).

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
regenerated. **OPEN:** does Janus scaffold this file with `TODO` cases the
*first* time it's needed, or does the human create it from scratch? Either
way, once it exists, Janus never touches it again.

**Rule — the two button verbs are not the same path:** `on_press` values
route here, through the human's own switch. `navigate` values never reach
this file — they're handled directly by `janus_switch_screen()` inside the
fixed runtime library, because Janus itself understands "go to this
screen" (it validated the target at parse time); it does not understand
what "reboot" means.

---

## Stage 6 — Input dispatch (deferred, contract sketched, not built)

**Receives:** raw hardware events (touch coordinates / encoder deltas /
GPIO edges) + read-only access to the active screen's
`janus_widget_desc_t[]` (for `geometry`, hit-testing) and `focus_order`
(for encoder/button traversal — the field exists in the struct above but
nothing populates or reads it yet).

**Produces:** a resolved `(widget, janus_action_t)` pair, handed to the
same place `on_press`/`navigate` handling already goes (Stage 5).

**Owner:** fixed library, one module per modality
(`janus_input_touch.c` / `janus_input_encoder.c` / `janus_input_buttons.c`)
— **none exist yet**. Not required before Stage 0–5 work; can land later
without touching any earlier stage.

---

## Stage 7 — harpia's downstream codegen (external)

**Receives:** the full assembled schema — human root `.harpia` (which
`import`s Stage 3a's `janus_generated.harpia`) — via harpia's own `harpia`
CLI, specifically `ZmqAdapter` for this target (REST/gRPC are out per
`Janus.md`'s "Targets" section).

**Produces:** transport + (de)serialization code for every bound message.

**Owner:** external — entirely harpia's own toolchain, regenerated by
running `harpia`, not Janus.

**OPEN, unresolved, blocks real generator code:** whether `ZmqAdapter`
emits anything usable as the plain C struct that Stage 4's
`janus_screen_desc_t.bound_struct` and Stage 3b's `.field_offset =
offsetof(...)` assume exists — harpia's main C++ backend is protobuf-based
and explicitly too heavy for this target. Needs a research pass into
`harpia/ZmqAdapter/` before Stage 3b/4 get real implementations.

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
| `src/main.c` | human (or Janus-scaffolded once — **OPEN**) | no |
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

**Runtime call flow, on interaction (once Stage 6 exists):**
4. The active input module resolves an event to `(widget, action)`.
5. If `action` came from `on_press`: `janus_handle_action(action)` — the
   human's file, Stage 5.
   If it came from `navigate`: `janus_switch_screen(&app, target_index)`
   — the fixed library sets `active_screen` and re-renders the *new*
   screen from *its* geometry table. Only one screen's widgets are ever
   live, matching the ~2 KiB transient-buffer budget.
   If the widget is a `box` header: `janus_toggle_box(widget)` — flips
   that widget's local expand bit and re-renders just that subtree, using
   `geometry` vs `geometry_collapsed` — a tile-scoped redraw, not a full
   screen repaint, per DESIGN.md's tiling model.

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
