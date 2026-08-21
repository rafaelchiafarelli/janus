# Janus — architecture contract

Companion to `Janus.md` (the narrative design doc) and `docs/architecture.drawio`
(the picture). This file is the reference: for every stage in the pipeline,
exactly what it receives, what it produces, who owns the output, and whether
it gets regenerated. Read `Janus.md` first for *why*; this file is *what*,
precisely enough to implement against.

Status (updated 2026-08-20): Stages 1–8 are implemented and tested for the
embedded-C target (see each stage's "Implementation status" / "Owner" —
`examples/host_demo` builds and runs end-to-end, including Janus-generated
bindings and glyph rendering for both static text and live bound string
values, verified against the real generated output today). **Embedded C
is now Janus's only target** — the JS/Node target (Janus.md's "2. JS/Node
web frontend") was dropped the same day: harpia's own generated REST/gRPC
already gives direct backend access, so a generated frontend on top of it
wasn't needed. **Stage 6 (encoder/button input dispatch) is now also
implemented** — touch, encoder, and next/prev/select push buttons all
work, sharing one focus core (`janus_input_focus.c`); see Stage 6 below.
Still genuinely open: the pixel-format rework for non-mono display
controllers (RGB565/e-paper) and per-controller driver bodies, both
blocked on real hardware to build against — see Janus.md's Open
Questions. Where a shape isn't nailed down, it's marked **OPEN**.

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
  display: { size: {w: 240, h: 320}, color: mono }   # optional — see Stage 1/2
  input: { modality: touch }   # optional, default touch — see Stage 1/6/8
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

**`display:` (optional, added 2026-08-20; `bus`/`controller` added same
day)** — `app.yaml`'s `size: {w, h}` + `color: mono|gray|rgb565` (default
`mono`) + optional `bus: spi|i2c|parallel` + optional `controller:` one of
`st7789`/`st7789v`/`ili9341`/`ili9341v`/`hx8357`/`gc9a01`/`ssd1306`/
`sh1106`/`il3820`/`il0373`, parsed into `App.display: DisplayConfig |
None`. `color`/`bus`/`controller` are each validated against their own
closed set here (parse-time, same as `bind.type`); `bus` and `controller`
are independent of each other and of the rest of `display:` (either, both,
or neither may be omitted). `size` bounds-checking against actual screen
content happens at Stage 2 (`check_fits_display`), not here — same split
as widget `size` above. `bus`/`controller` are a hardware **selection**
only — no driver body is generated from them; see Stage 3b below and
Janus.md's Open Questions for the human-owned/"boltable" decision.

**`input:` (optional, added 2026-08-20)** — `app.yaml`'s
`modality: touch|encoder|buttons` (default `touch`), validated against
this closed set, parsed into `App.input_modality`. Picks which of Stage
8's three `main_*.c.tmpl` scaffolds gets written (see Stage 8 below) —
unlike `display:`, this has no nested config object (just the one field),
so it's a plain `App` field, not a separate dataclass.

---

## IR data contract

The shared interchange format every later stage reads or writes. Nothing
downstream touches YAML directly — originally kept target-agnostic so a
JS/Node emitter could plug in as a third target later without reworking
the front half; that target was dropped (2026-08-20, see status above),
but the split is still the right shape for the two emitters that exist
(Stage 3a harpia, Stage 3b embedded-C).

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
class DisplayConfig:
    width: int
    height: int
    color: Literal["mono", "gray", "rgb565"] = "mono"
    bus: Literal["spi", "i2c", "parallel"] | None = None
    controller: Literal[
        "st7789", "st7789v", "ili9341", "ili9341v", "hx8357",
        "gc9a01", "ssd1306", "sh1106", "il3820", "il0373",
    ] | None = None
    # bus/controller are a selection only — no driver body is generated
    # from them (human-owned, see Stage 3b + Janus.md Open Questions)

@dataclass
class App:
    screens: list[Screen]
    nav: list[NavTarget] | None    # kind is always "tabs" in v1
    display: DisplayConfig | None  # None if app.yaml omits `display:`
    input_modality: Literal["touch", "encoder", "buttons"] = "touch"
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

**Display bounds check (`check_fits_display`, added 2026-08-20):** a
separate function from `layout_screen` — `layout_screen` itself stays a
pure per-`Screen` function with no `App`/display access, unchanged. Called
by `janus/cli.py` once per screen, after `layout_screen`, only when
`app.display is not None`. Compares that screen's now-computed
`root.geometry` against `display.width`/`height` and raises `ValueError`
on overflow — same "validate, don't silently clip" rule as Stage 1. Skipped
entirely (no check, no error) when `app.yaml` has no `display:` block —
today's behavior, unchanged.

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
  **`.focus_order` (Stage 6, added 2026-08-20):** `_assign_focus_order`
  walks each screen's root in pre-order — the same order
  `janus_runtime.c`'s `render_widget`/`janus_input_touch.c`'s
  `hit_test_widget` traverse at runtime, *not* `_emit_widget`'s own
  emission order (that one's post-order, so a container's children array
  is declared before the container references it — an unrelated C
  forward-declaration concern). "Focusable" is exactly the set touch
  already dispatches on (`box`, or a leaf with `on_press`/`navigate` set)
  — no new YAML field needed. Non-focusable widgets get
  `JANUS_FOCUS_NONE` (255).
- `janus_actions.gen.h` — one shared file: an enum with one value per
  distinct `on_press` string across *all* screens in the app (deduped).
  `navigate` values do **not** get an enum entry here — see Stage 5.
  Implemented: `emit_actions_header(app)`.
- `janus_app.gen.c` — the `janus_app_t` table: an array of pointers to
  every screen's `janus_screen_desc_t`, plus `nav_titles` (parallel array,
  `NULL` if `app.nav` is unset). Implemented: `emit_app_table(app)`;
  raises if `app.nav` doesn't cover every screen.
- `janus_display_config.gen.h` — only written when `app.display` is set:
  `JANUS_DISPLAY_WIDTH`/`HEIGHT` + `JANUS_DISPLAY_COLOR*`/`BUS*`/
  `CONTROLLER*` `#define`s (see Stage 2's display bounds check and
  Janus.md's "Display config" section). Implemented:
  `emit_display_config(display)`. Every known `color`/`bus`/`controller`
  value is always defined (so driver code can compare against any of
  them); the *selection* macro (`JANUS_DISPLAY_BUS`/`_CONTROLLER`) is only
  emitted when that field is set on `DisplayConfig` — `color` always picks
  one (defaults `mono`), `bus`/`controller` may stay unselected. Plain
  data for hand-written vendor driver code to consume — the fixed runtime
  library never reads it, and no driver body is generated from the
  selection (human-owned, see Janus.md Open Questions).

**`.bound_struct` resolution (v1: one message per screen).** Each
widget's `.bind.field_offset = offsetof({message}_t, {field})` assumes
`{message}_t` is a real, visible C struct type. `.bound_struct` needs an
*instance* of that type, though, and something has to give for Stage 4 to
read live values at all: `emit_screen()` collects the distinct
`Binding.message` names used by that screen's widgets
(`screen_bound_messages`) and requires there be at most one. With one,
it emits `.bound_struct = &{message}_instance` and
`render_screen_source` adds a conditional `#include "janus_bindings.gen.h"`
(built in Python, same pattern as `emit_app_table`'s `titles_ref`, no new
templating logic). With zero bindings, `.bound_struct = NULL` and the
include is omitted. With more than one distinct message, `emit_screen()`
raises `ValueError` — matches Stage 1's "validate, don't silently
default" philosophy.

**`{message}_t` / `{message}_instance` themselves are now Janus-owned,
not human-written** (`emit_bindings_struct.py`, implemented per Stage 7's
follow-up research pass below): the type/field-offset side of Stage 7's
old open question is resolved — harpia's `ZmqAdapter` was confirmed to be
structurally incapable of producing a plain C struct (protobuf's C++
classes only), so Janus generates its own from the same `Binding`s
`emit_harpia.py` already walks. What's still human-owned is *populating*
the generated instance with real values at runtime — the struct itself
only zero-inits (see Stage 7 below for why, and where the split falls).

**Implementation status:** implemented and tested: the raw declarations
(`emit_screen`/`emit_actions_header`/`emit_app_table`/
`emit_bindings_header`/`emit_bindings_source`), the `.tmpl` wrapper
turning them into complete standalone files with `#include`s and header
guards (`emit_files.py`, `janus/templates/*.tmpl`), and
`generate.write_project` writing them to disk with
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
widget, `#include "janus_bindings.gen.h"` when it has any `bind` — and
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
    JANUS_WIDGET_DIVIDER, JANUS_WIDGET_TOGGLE, JANUS_WIDGET_BADGE, JANUS_WIDGET_SLIDER,
} janus_widget_kind_t;

typedef enum { JANUS_FIELD_NONE, JANUS_FIELD_INT, JANUS_FIELD_INT64, JANUS_FIELD_FLOAT, JANUS_FIELD_STRING } janus_field_type_t;

typedef struct { int16_t x, y, w, h; } janus_rect_t;

typedef int16_t janus_action_id_t;   /* see note below — not janus_action_t */

/* Stage 6: shared across every input modality — originally declared only
 * in janus_input_touch.h, moved here once encoder/buttons needed the
 * identical shape, so no modality module depends on another. */
typedef enum {
    JANUS_INPUT_NONE, JANUS_INPUT_ACTION, JANUS_INPUT_NAVIGATE, JANUS_INPUT_TOGGLE_BOX,
} janus_input_kind_t;

typedef struct {
    janus_input_kind_t kind;
    const struct janus_widget_desc *widget;
    janus_action_id_t action;
    int16_t navigate_target;
} janus_input_result_t;

#define JANUS_FOCUS_NONE ((uint8_t)255)   /* .focus_order sentinel: "not focusable" */

typedef struct {
    uint16_t field_offset;         /* offsetof() into bound_struct; 0 if unbound */
    janus_field_type_t field_type;
    float range_min, range_max;    /* progress/gauge only */
} janus_bind_t;

typedef struct janus_widget_desc {
    janus_widget_kind_t kind;
    const char *id;
    const char *static_text;           /* authored Widget.text, baked in; NULL if none or bound */
    janus_rect_t geometry;             /* also the "expanded" rect for box */
    janus_rect_t geometry_collapsed;   /* box only, ignored otherwise */
    bool initial_expanded;             /* box only, ignored otherwise — baked from Widget.default_expanded */
    janus_bind_t bind;
    janus_action_id_t action;          /* on_press only; 0 otherwise (== JANUS_ACTION_NONE by convention) */
    int16_t navigate_target;           /* navigate only; index into janus_app_t.screens, -1 otherwise */
    uint8_t focus_order;               /* encoder/button traversal order, or JANUS_FOCUS_NONE; touch ignores this */
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
void janus_switch_screen(janus_app_t *app, uint16_t screen_index);   /* used by navigate; clears focus first */
void janus_toggle_box(const janus_widget_desc_t *box);               /* re-renders just that subtree */
bool janus_box_is_expanded(const janus_widget_desc_t *box);          /* reads the state above; Stage 6 hit-testing needs it */

/* Stage 6: encoder/button focus highlight — see Stage 6 below. */
void janus_set_focus(const janus_widget_desc_t *widget);   /* NULL clears it */
const janus_widget_desc_t *janus_get_focus(void);
```

**Why `janus_action_id_t`, not `janus_action_t`, on the descriptor.**
`janus_action_t` is defined by the *generated*, per-project
`janus_actions.gen.h` (Stage 3b) — but `janus_widget_desc_t` is defined
once, in this fixed header, with no guarantee any per-project file has
been `#include`d first. Typing the field as a generic `int16_t` id
(enum constants convert to it implicitly, no cast needed at the
generation site) keeps the fixed library fully independent of any
generated enum. Stage 6 casts back to `janus_action_t` right before
calling `janus_handle_action`.

**Box collapse state.** `janus_widget_desc_t` instances are
`static const` arrays baked at generation time — there's nowhere in them
to store a *mutable* "currently expanded" bit. `janus_runtime.c` keeps a
small fixed-capacity table mapping descriptor pointer → current expanded
bit (descriptors have program-lifetime-stable addresses, so pointer
identity is a safe key), seeded from `.initial_expanded` the first time
a box is rendered and flipped by `janus_toggle_box`.

**Focus state (Stage 6, added 2026-08-20).** Same shape as box state, but
simpler: `janus_widget_desc_t` still has nowhere to hold a mutable "am I
focused" bit, but only one widget is ever focused at a time (across the
whole app, not per-screen), so `janus_runtime.c` needs just one static
pointer (`g_focused_widget`), not a table. `janus_set_focus(w)` compares
`w` against it, redraws the previous widget unfocused and `w` focused
(each via the normal `render_widget` for that one widget — no dedicated
"focused" draw path; `draw_button`/`draw_box_header` just check `w ==
g_focused_widget` internally and add a thin border via `draw_focus_ring`
when true), and updates the pointer. `janus_switch_screen` calls
`janus_set_focus(NULL)` before rendering the new screen — without this, a
focus pointer from the outgoing screen's static widget array would get
redrawn on top of the incoming screen's freshly rendered content. Only
`button` (unbound in v1, per Janus.md's catalog) and `box` are ever
focusable, so the redraw never needs `bind` data — `read_bound_value`/
`read_bound_string` are never called from this path.

`janus_runtime.c` implements traversal + tiling + one internal
`draw_<kind>()` per widget kind, dispatched by `kind` — this is where
DESIGN.md's actual rendering logic lives, finally for real (not the
deleted Copilot stub). Scope note: only the synchronous path
(`draw_area_sync`) is driven by the traversal so far — `draw_area_async`/
`display_busy` are declared (any driver must still provide them) but not
yet called by anything; polled/non-blocking scheduling is real future
work. Leaf rendering is a kind-distinct solid fill of `geometry`, except
`progress`/`gauge` (fraction of range) and `checkbox` (checked/unchecked)
which read the *live* bound value and vary the fill accordingly — the
concrete difference from the deleted Copilot stub's "empty buffer
regardless of screen contents."

**Glyph rendering (2026-08-19, slice 1; slice 2 completed 2026-08-20) —
both static text and live bound strings.** `label`/`header`/`button`/
box-header draw real characters over that same solid fill when
`.static_text` is non-`NULL`, via a fixed-library module (`janus_font.h`/
`.c`, alongside `janus_input_touch.c` as another pluggable piece — same
"one file per concern" pattern): a 5×7 bitmap font covering space +
`A`-`Z` only (27 glyphs; lowercase case-folds onto the uppercase glyph,
digits/punctuation have no glyph yet — a real coverage gap, not a bug,
widened by adding rows to `janus_font.c`'s table, nothing else).
`draw_string()` blits left-aligned, vertically centered, and clips (never
wraps or shrinks the font) once a character would run past the widget's
`geometry` — Janus never auto-sizes text at generation time (`Janus.md`'s
deferred auto-sizing note), so overflow is an expected v1 case:
`examples/host_demo`'s own `diagnostics_box` (24px wide) clips
"Diagnostics" down to "Diag" for exactly this reason, verified against
the real generated output, not just unit tests.

Stage 3b bakes `.static_text` from `Widget.text` for every widget
(`emit_embedded_c.py`'s `_widget_init`) — previously `Widget.text` was
parsed at Stage 1 and never read again by anything downstream, a gap
closed as a prerequisite for glyph rendering to have anything to draw.

**Slice 2 (2026-08-20): `label`/`header` now also draw a live bound
string when there's no authored `text:`.** `janus_runtime.c` gained
`read_bound_string()` — dereferences the bound struct's `const char *`
field directly (the field itself *is* a pointer, per
`emit_bindings_struct.py`'s `_C_TYPE["string"] = "const char *"`, unlike
`read_bound_value`'s numeric types which reinterpret inline bytes) and
returns it to `draw_string()` unchanged. **The "truncation/lifetime
story" earlier flagged as this slice's open risk turned out to be a
non-issue**: `draw_string()` already blits and clips character-by-character
straight from the source pointer (see above), so an arbitrary
runtime-length string never needs a length known upfront or a copy into a
fixed buffer — no new machinery was needed beyond wiring the read.
Lifetime is a firmware concern like any other bound value: the pointer
must stay valid at render time, same as `device_instance.name` being set
once in `examples/host_demo/host_main.c` before the first render.
Zero-initialized bindings instances (Stage 7) hold `NULL` here until
firmware populates them, and `draw_string()` already no-ops on `NULL`, so
an unpopulated bound string renders as the plain fill — unchanged
behavior from before this slice, not a special case. `static_text` wins
if a widget somehow has both (nothing at parse time forbids it) — a
deterministic tie-break, not new validation.

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

## Stage 6 — Input dispatch (touch, encoder, buttons — all implemented 2026-08-20)

**Receives:** depends on modality — a touch point (`x, y`) for touch; a
rotate delta or click for encoder; a NEXT/PREV/SELECT edge for buttons.
Every modality gets read-only access to the active screen's
`janus_widget_desc_t[]` (`geometry` for touch's hit-test, `focus_order`
for encoder/buttons' walk).

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
(`on_press`). All three modalities produce this identical shape and get
dispatched identically by the caller (`main.c`'s event loop) — confirming
the original sketch's core claim that only the "which widget" front-end
differs per modality.

**Touch — hit-testing** (`janus_touch_hit_test`, `janus_input_touch.c`):
point-in-rect against the already-baked absolute geometry, deepest match
wins. `box`'s `geometry_collapsed` doubles as its header hit-region —
`layout.py` already sets it to exactly the header strip's bounds
regardless of current expand state, so a point inside it is always a
header tap (→ `TOGGLE_BOX`), with no separate header-height constant
needed at runtime. A point elsewhere in `geometry` only recurses into a
box's children when `janus_box_is_expanded(box)` is true — a collapsed
box's children are never hit-testable, matching Stage 4's own render-time
skip. `column`/`row`/`radiogroup` have no rect of their own action-wise
and just recurse. A leaf hit with neither `navigate_target` nor `action`
set (a plain label, an unwired checkbox) is a deliberate, defined miss
(`JANUS_INPUT_NONE`) — nothing to dispatch. If a widget somehow has both
`navigate` and `on_press` set, `navigate` wins (an arbitrary but
documented tie-break; nothing currently prevents authoring both — same
tie-break `janus_focus_activate` below uses).

**Touch driver contract** (`janus_input_touch.h`, parallel to the output
driver contract): `bool janus_touch_poll(int16_t *x, int16_t *y)` —
vendor/host-provided, non-blocking, mirrors `display_busy()`'s polling
style. Returns true and fills `x`/`y` once per new touch.

**Encoder/buttons — shared focus core** (`janus_input_focus.h`/`.c`, new):
`janus_focus_move(screen, delta)` and `janus_focus_activate(screen)`,
used identically by both modalities — encoder rotation and button
NEXT/PREV both call `janus_focus_move` (`+1`/`-1`), encoder click and
button SELECT both call `janus_focus_activate`. Internally, a
depth-first, left-to-right walk (`walk_focusable`) mirrors
`hit_test_widget`'s own traversal and its collapsed-box skip rule exactly
— `focus_order` was baked assuming every box is reachable, so the walk
has to apply the same runtime skip touch does, or a collapsed box's
children would be silently focusable while invisible. `janus_focus_move`
computes the currently-focused widget's position by walking once
(`focus_position`), steps by `delta` with wraparound at either end, and
resolves the target position with a second walk (`widget_at`) — two
small bounded recursions per input event (human-paced, not per-frame),
no arrays, no malloc. `delta == 0` is the documented idiom for
"(re-)establish focus on this screen" (nothing focused yet always resolves
to index 0 regardless of delta) — `main_encoder.c.tmpl`/
`main_buttons.c.tmpl` call it once right after the first
`janus_render_screen`, and again after every `JANUS_INPUT_NAVIGATE`
dispatch. `janus_focus_activate` resolves whatever `janus_get_focus()`
currently returns using the identical box/navigate/action rules touch's
leaf case uses, but returns `JANUS_INPUT_NONE` if that widget isn't
actually reachable on `screen` right now (stale — e.g. its box was
collapsed since it was focused, or focus belongs to a screen that's since
been switched away from).

**Encoder/buttons driver contracts** (`janus_input_encoder.h` /
`janus_input_buttons.h`): header-only, no `.c` — same shape as
`draw_area_sync`/`janus_touch_poll`, vendor/host-implemented, not fixed-
library logic. `bool janus_encoder_poll(janus_encoder_event_t *event,
int16_t *delta)` (`JANUS_ENCODER_ROTATE`/`_CLICK`); `bool
janus_buttons_poll(janus_button_event_t *event)`
(`JANUS_BUTTON_NEXT`/`_PREV`/`_SELECT`). Push buttons were scoped as
next/prev/select specifically because that's what lets them reuse
`janus_focus_move`/`janus_focus_activate` almost for free — a
direct-mapped GPIO→action scheme (bypassing focus, one button hardwired
to one action) was considered and rejected as a separate, larger,
human-authored feature (closer to `janus_handle_action` territory than
anything Stage 3b would derive from the screen spec).

**Owner:** fixed library, one module per modality — `janus_input_touch.h`
/ `.c`, `janus_input_focus.h`/`.c` (shared core), `janus_input_encoder.h`,
`janus_input_buttons.h` (driver contracts, header-only) all exist. Which
one a project scaffolds into `main.c` is `app.input_modality` (Stage 1's
`input:`), read at Stage 8, not here — this stage's own code is identical
regardless of which modality ends up calling it.

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
The `janus_bindings.h` convention `examples/host_demo` used to hand-write
(Stage 3b's `.bound_struct` resolution) was pointing the right direction
but for the wrong reason — it shouldn't be a permanent human/vendor
hand-write standing in for a harpia output that will never arrive; it
became **another Janus-generated artifact**, alongside
`janus_generated.harpia`: a plain C struct per bound message, generated
directly from the same `Binding`s `emit_harpia.py` already walks (same
field names/types, independently emitted in two languages for two
targets — harpia's C++/protobuf schema for desktop/server, Janus's own
plain C struct for embedded — not one consuming the other's output).
**Implemented** the same day this was scoped — see the follow-up
research pass and Stage 3b's `.bound_struct` resolution above.

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

**Follow-up research pass (2026-08-19), scoping the Stage 3b increment
above — IMPLEMENTED same day, see Stage 3b's `.bound_struct` resolution
above.** Confirmed the "no plain C struct" finding one level deeper than
`ZmqAdapter` — `harpia/protoFile/ProtoCompiler.py:63` invokes
`protoc -I <root> --cpp_out <scratch>` unconditionally; there is no
`--c_out`/nanopb/protobuf-c path anywhere in harpia (checked every `.py`
in the repo). This isn't an adapter-level choice that a different backend
could route around — it's the one protoc invocation every message goes
through, full stop.

The generated-struct increment turns out to need no new design decisions,
only new code, because Stage 3b already commits to its shape without
defining it:
- `emit_embedded_c.py`'s `_widget_init` (line 134) already emits
  `offsetof({message}_t, {field})`, and `emit_screen` (line 221) already
  emits `&{message}_instance` — both reference a type/symbol that doesn't
  exist yet anywhere Janus generates.
- The field walk already exists too: `emit_harpia.py`'s
  `_collect_bindings` builds exactly `{message: {field: Binding}}`,
  currently used only for the `.harpia` Include. A new
  `emit_bindings_struct.py` would call the same walk and map
  `Binding.type` to a C type — a closed 4-entry table (`string` →
  `const char *`, `int` → `int`, `int64` → `int64_t`, `float` → `float`),
  already validated at Stage 1 (`_VALID_BIND_TYPES` in `dsl_yaml.py`) and
  already mirrored in the runtime's `janus_field_type_t` enum, so there's
  no new type system to invent.
- Output would be `janus_bindings.gen.h`/`.gen.c`, matching every other
  Stage 3b output's `.gen.` convention, replacing today's hand-written
  `examples/host_demo/janus_bindings.h`/`.c`. `emit_files.py:44`'s
  hardcoded `#include "janus_bindings.h"` moves to the `.gen.h` name at
  the same time.
- New open question this pass surfaces, not answered by architecture.md
  before now: **initial values.** `janus_bindings.c` today hand-inits
  demo data (`.battery_level = 42`, ...). A Janus-generated struct can
  only zero-init (`= {0}`) — real values become firmware's job to
  populate at runtime (e.g. a ZMQ receive callback), the same
  Janus-generates-shape/human-owns-behavior split Stage 5 and Stage 8
  already use elsewhere. Doesn't block the increment, just means
  `device_instance`'s current demo values move from `janus_bindings.c`
  into `src/janus_actions.c` or `src/main.c` (human-owned) once this
  lands.
- **Does not unblock string rendering.** `read_bound_value` in
  `janus_runtime.c` returns `0.0` for `JANUS_FIELD_STRING` regardless of
  where the struct comes from — that's the Stage 4 glyph-engine gap
  (`Janus.md`, "Stage 2+"), a separate unbuilt piece. Generating the
  struct makes `label`/`header` bindings *compile*, not *render*.

---

## Stage 8 — Firmware assembly

This is the "how does it all actually get compiled" question.

**Compilation units and who owns each:**

| file | owner | regenerated? |
|---|---|---|
| `runtime/embedded_c/src/janus_runtime.c` | Janus (fixed library) | no — only on Janus version upgrade |
| `runtime/embedded_c/src/janus_input_touch.c` / `janus_input_focus.c` | Janus (fixed library) | no — only on Janus version upgrade |
| `build/generated/{screen}_screen.gen.c` | Janus | yes, every build |
| `build/generated/janus_actions.gen.h` | Janus | yes, every build (cheap — just names) |
| `build/generated/janus_app.gen.c` | Janus | yes, every build |
| `build/generated/janus_bindings.gen.h/.c` | Janus | yes, every build (struct shape only — instance zero-inits; a human file populates real values at runtime) |
| `build/generated/janus_display_config.gen.h` | Janus | yes, every build (only if `app.display` set) |
| `janus_generated.harpia` → harpia's own codegen output | harpia (external) | yes, via `harpia` CLI |
| `src/janus_actions.c` | human | no |
| `src/main.c` | human (Janus-scaffolded once, `scaffold_main_c` — one of `main_touch.c.tmpl`/`main_encoder.c.tmpl`/`main_buttons.c.tmpl`, picked by `app.input_modality`) | no |
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
4. `main()`'s event loop polls whichever modality `app.input_modality`
   scaffolded (`main_touch.c.tmpl` polls `janus_touch_poll(&x, &y)` then
   calls `janus_touch_hit_test`; `main_encoder.c.tmpl`/
   `main_buttons.c.tmpl` poll `janus_encoder_poll`/`janus_buttons_poll`,
   route rotate/NEXT/PREV to `janus_focus_move` directly — no
   `janus_input_result_t` for those, nothing to dispatch yet — and route
   click/SELECT to `janus_focus_activate`) (Stage 6).
5. The scaffolded `main.c` switches on the result's `kind` — this is the
   one place all three modalities' outcomes meet, and it's the human's
   file (editable after scaffolding) that does the switching, not the
   fixed library:
   - `JANUS_INPUT_ACTION`: `janus_handle_action((janus_action_t)hit.action)`
     — the human's file, Stage 5.
   - `JANUS_INPUT_NAVIGATE`: `janus_switch_screen(&app, hit.navigate_target)`
     — the fixed library sets `active_screen`, clears focus, and
     re-renders the *new* screen from *its* geometry table. Only one
     screen's widgets are ever live, matching the ~2 KiB transient-buffer
     budget. Encoder/button scaffolds follow this with one more call —
     `janus_focus_move(new_screen, 0)` — to establish focus on the screen
     that's now active; touch doesn't need this step.
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
| `janus_bindings.gen.h/.c` | Janus | every run (zero-init only — see Stage 8) |
| `janus_display_config.gen.h` | Janus | every run (only if `app.display` set) |
| `runtime/embedded_c/*` | Janus (fixed library) | on Janus upgrade only |
| `src/janus_actions.c` | human | never |
| `src/main.c` | human | never |
| `src/display_driver.c` | human/vendor | never |
| harpia's `ZmqAdapter` output | harpia (external) | every `harpia` run |
