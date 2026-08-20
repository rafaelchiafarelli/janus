# Janus — a pre-harpia GUI schema + code generator

> **Status (updated 2026-08-19): the embedded-C target is implemented and
> working end-to-end**, not "design only" anymore — see `architecture.md`'s
> per-stage "Implementation status." Still open: encoder/button input,
> the glyph/font engine ("Stage 2+" below), and the JS/Node target. This
> file is still meant to let a fresh session pick this project up quickly —
> read it whole before writing any code. A visual companion to the
> "Embedded-C code generation architecture" and "Input / event dispatch"
> sections lives at [`docs/architecture.drawio`](docs/architecture.drawio)
> (open in draw.io / diagrams.net). The precise receives/produces contract
> for every pipeline stage — IR shape, generated C struct layout, ownership
> rules, build/link graph — lives in [`architecture.md`](architecture.md).

Janus is a screen/widget DSL and generator that runs **before** harpia in a
project's pipeline. You author a UI (screens, widgets, data bindings); Janus
produces, from that one spec: (1) a `.harpia` schema Include file and (2)
runnable UI code for one or more targets.

Named for the two-faced Roman god of doorways and transitions: one face
looks toward the UI/screen spec, the other toward harpia's data model —
Janus is the gate between them, not a merger of the two.

## Origin

harpia (a separate project — a `.harpia` → C++ code generator: protobuf,
SOCI-backed SQL/CRUDL, JSON/XML/SOAP/REST/gRPC/ZMQ, generated tests) needed
a GUI generator for small/constrained devices. GitHub Copilot's first
attempt lived on harpia's `gui-dsl-prototype` branch (`GuiAdapter/`).
harpia's `NEXT_SESSION.md` has a one-line pointer back to this file.

**Correction (2026-08-19):** earlier drafts of this doc claimed
`GuiAdapter/` was deleted from harpia, both locally and on `origin`. It
isn't — confirmed still present in full on harpia's `dev` branch (current
`HEAD`) during Stage 7's research pass. Doesn't change anything decided
below (the verdict, the "why not part of harpia" reasoning, and the
salvage plan all stand on their own merits regardless), but "Salvageable
from the deleted Copilot branch" further down is stale in the same way —
treat both as "not deleted, just not reused," not "gone."

## Verdict on the Copilot prototype (why it wasn't reused as architecture)

- It lived at `GuiAdapter/` — named and located like one of harpia's real
  backends (`JsonAdapter/`, `XmlAdapter/`, `ZmqAdapter/`, `Database/`), all
  of which parse the *same* already-built `.harpia` Message/Variable tree —
  but its generator (`GuiAdapter/tool/generator.py`) parsed a totally
  separate, bespoke YAML file and never touched harpia's lexer/parser at
  all. It looked integrated; it wasn't.
- Its actual code was a self-described "Stage 1" stub:
  `gui_render_blocking` drew an empty, unfilled tile buffer regardless of
  screen contents — no widget traversal, no font rendering, no event
  dispatch. Promised in its own `DESIGN.md` §11–12 but never delivered: a
  host mock driver for testing, any CMake integration, any golden tests.
- Its `DESIGN.md` (runtime contract, memory budget, tiling model, driver
  contract) is genuinely solid and worth carrying forward almost as-is into
  Janus's embedded-C target spec — see "Salvageable" below. The problem was
  never the runtime design, it was the integration story.

## Why this isn't part of the harpia repo or its grammar

- harpia's `.harpia` lexer is one flat regex-token list
  (`LexicalAnalizer/LexicalAnalyzer.py`) plus a brace-slicing parser
  (`message/Message.py: Process()`) built for flat field-list declarations.
  Even *one* level of message nesting (sub-messages) already needed heavy
  special-casing (`harpia.process.md` §1.3.0.3.1–.3: mandatory unique FK,
  public/private inheritance rules, "hash" naming when unnamed, no
  modifiers allowed on sub-messages).
- GUI layout is inherently recursive (containers within containers),
  stateful (visibility conditions, animations), and needs richer
  expressions (data-binding, format strings, event routing) than harpia's
  declaration grammar was ever designed for. Bolting that onto the
  brace-slicer would multiply the sub-message special-casing problem, not
  reuse it cleanly — small things would stay simple, complex things would
  become exponential.
- Target runtime mismatch: harpia's generated C++ output assumes protobuf +
  gRPC + SOCI + Crow — none of which fit an 8-bit-class, ~2 KiB-RAM
  embedded target. Coupling the two pipelines would force the embedded
  runtime to either drag in that weight or live as an awkward exception
  inside a pipeline that assumes it's there.

## What Janus is

A standalone tool/repo. Treats harpia purely as a downstream dependency —
never a shared codebase, never a shared grammar.

**Input:** a screen/widget specification (concrete syntax not yet decided —
see Open questions). A screen declares widgets (label/button/image/
progress/...), their layout, and data bindings to fields, e.g.
`bind[user.name]`.

**Output, from that one spec:**
1. A harpia Include file (`.harpia` syntax, e.g. `janus_generated.harpia`)
   declaring/extending the messages referenced by widget bindings — the
   schema half.
2. UI runtime code for one or more targets (see Targets) — the
   rendering/behavior half.

## Schema ownership — the key design decision

The generated `.harpia` Include file is used exactly like any other harpia
`Include/*.harpia` file (precedent: `HarpiaTest/test.harpia` imports
`HarpiaTest/Include/file{1,2,3}.harpia`; see `harpia.process.md` §1.3.0.7–.8
for cross-file message reference rules). A project's real, human-owned root
`.harpia` file adds one `import` line pointing at Janus's output and
references its messages normally.

This resolves the ownership/drift problem cleanly:
- Janus's generated Include file is **wholly owned by Janus** — regenerated
  and overwritten in full on every run. No partial diff, no merge logic.
  Similar in spirit to how harpia treats its own generated C++ output —
  though correcting a claim from an earlier draft of this doc: harpia does
  *not* have a `write_if_different`/`prune_stale_outputs` utility (verified
  against the actual source — no such names exist anywhere in the repo).
  What harpia actually does is `shutil.rmtree()` the output directory then
  unconditionally `open(path, "w").write(...)` every file on every run
  (`main.py` around the `testDestination` cleanup, before regeneration) —
  full wipe-and-regenerate, not a smart diff/prune step. Janus's own writer
  should still *do* a real content-diff-before-write (see the embedded-C
  codegen section below) — that's a deliberate Janus design choice, not
  something ported from harpia.
- The human's root `.harpia` (and any other hand-written Include) is
  **never written to** by Janus. Fields no widget touches (audit columns,
  internal FKs, anything DB-only) stay the human's responsibility,
  declared in their own files.
- Consequence: Janus is a schema *contributor*, never a schema's sole
  author. A project's full data model = human-owned messages +
  Janus-generated Include, assembled the normal harpia way.
- Confirmed detail worth relying on: only the harpia ROOT file's own md5
  hash tags generated filenames project-wide — editing/regenerating an
  Include file never perturbs that hash. Regenerating Janus's Include on
  every screen-spec change is cheap and doesn't cascade renames through the
  rest of the harpia-generated output.

## Targets

Two targets, deliberately asymmetric — they don't share a backend story.

### 1. Embedded C runtime (the original ask)

For small/constrained devices — the original motivating use case.

- Carries forward Copilot's `DESIGN.md` almost as-is: 240×320-class
  displays, ~2 KiB RAM budget for transient buffers, blocking AND
  non-blocking (tiled, polled) rendering, a small driver contract
  (`draw_area_sync`/`draw_area_async`/`display_busy`), no malloc, never
  disables global IRQs, pre-generated rotated assets for 0/90/180/270.
- Needs its own transport — REST/gRPC are too heavy for this class of
  device. ZMQ is the natural fit: harpia already generates ZMQ
  push/pull/pub-sub (`ZmqAdapter`). A prior harpia session already scoped
  embedded reachability: ESP32 is plausible as a libzmq host; ATmega2560-
  class chips can't run libzmq at all regardless of transport choice; ZMQ's
  CURVE security doesn't compose with ESP-IDF's native mbedTLS anyway
  (that's a separate, harder problem, noted in harpia's own
  `NEXT_SESSION.md` — don't reopen it here).
- Not solved yet: real widget rendering (Copilot's runtime is a stub with
  no traversal logic), font/glyph atlas packing, asset compression — all
  flagged as "Stage 2+" in the original `DESIGN.md` and still true.

### 2. JS/Node web frontend

- Rides for free on harpia's already-generated REST (Stage 12) and gRPC
  (Stage 13) endpoints, including their Stage-5 credential gating
  (`X-User`/`X-Pswd` headers or gRPC metadata) — no new backend surface
  needed.
- This is "visual access to the backend": a Node/JS app generated from the
  same screen spec, calling the REST/gRPC API a normal harpia project
  already emits for any bound message.

## v1 decisions (settled 2026-08-16)

- **First target: embedded C.** The original ask, and the harder of the two
  (real widget rendering, transport, font/glyph work are all still unbuilt)
  — scoped first rather than deferred, per doc step 2. JS/Node stays
  designed-for (see IR below) but unimplemented until embedded C works
  end-to-end for a trivial one-screen example.
- **Toolchain: Python**, matching harpia's own (lexer/parser, generator
  scripts). Codegen mechanism specifically: harpia-style `.tmpl` files +
  `str.format()` for file skeletons, with Python-built string fragments for
  repeated sections — no templating engine (Jinja2/Mako) anywhere in
  harpia, so Janus doesn't introduce one either. See "Embedded-C code
  generation architecture" below for how this applies to Janus specifically.
- **Pipeline is target-agnostic through an IR**, so a JS/Node emitter can
  later plug into the same tree without reworking the front half:
  `screen.yaml → parse → IR (Screen/Widget/Binding tree) → layout pass
  (fills in geometry) → emit(harpia Include) / emit(embedded-C)`.
- **Concrete v1 DSL: YAML**, explicit bindings, container-based layout with
  geometry computed by Janus at generation time (not authored, not computed
  on-device). Verified against real harpia syntax in
  `harpia/HarpiaTest/test.harpia` and `LexicalAnalizer/LexicalAnalyzer.py`
  (`message name{ type field; ... };`, types `int`/`int64`/`float`/
  `string`/`map<K,V>`, modifiers `optional`/`required`/`unique`/
  `repeteable`/`pagination[N]`):

  ```yaml
  # screen.yaml
  screen: UserProfile
  layout: column
  children:
    - kind: label
      id: name_label
      bind: { message: user, field: name, type: string }
    - kind: row
      children:
        - kind: label
          id: battery_caption
          text: "Battery:"
        - kind: progress
          id: battery_bar
          bind: { message: user, field: battery_level, type: int }
          size: { w: 80, h: 12 }
  ```

  generates `janus_generated.harpia`:

  ```
  message user{
      string name;
      int battery_level;
  };
  ```

  Rules this encodes:
  - Bindings are **flat and explicit** — every `bind` names `message`,
    `field`, and `type` directly; Janus never infers a harpia type from
    widget kind. No sub-messages emitted in v1 (sidesteps process.md
    §1.3.0.3's FK/naming special-casing entirely).
  - Multiple widgets binding to the same `message`/different `field`s merge
    into one `message` block — the one piece of real merge logic in the
    harpia emitter.
  - Widget **position** is never authored — containers (`layout: column` /
    `row`) stack children, and Janus's layout pass computes absolute
    `{x, y, w, h}` per widget, baking it into the embedded-C output as
    constants. The device draws precomputed rects; it never runs a layout
    algorithm at runtime (keeps the no-malloc/~2 KiB budget intact, same
    "push work to build time" spirit as the doc's pre-rotated 0/90/180/270
    assets).
  - Widget **size** is split by whether the dimension is content-driven or
    an arbitrary drawing choice — a correction made once the layout pass
    was actually implemented and the rule as first stated ("every leaf
    needs explicit size") turned out to contradict every worked example in
    this doc, which never bothered sizing a `label`/`header`/`button`/
    `checkbox`/`radiobutton` but always sized `progress`/`gauge`/`image`/
    `led`. `progress`, `gauge`, `image`, `led`, `badge`, `slider`
    **require** explicit `size` — their dimensions are a real design
    choice Janus can't guess. `label`, `header`, `button`, `checkbox`,
    `radiobutton`, `divider`, `toggle` get a fixed v1 placeholder default
    per kind when `size` is omitted (overridable), standing in until real
    font-metric auto-sizing exists (see auto-sizing note below).

## v1 widget catalog

Containers (structural, no harpia output):

| kind | notes |
|---|---|
| `column`, `row` | pure layout, build-time geometry |
| `box` | collapsible; Janus precomputes **two** geometry tables (expanded/collapsed) at build time — the device holds one runtime state bit per `box` and switches which precomputed table it draws from, never running layout math on-device. Collapse state is local UI state, not bound to a harpia field; `default_expanded` is authored. |
| `radiogroup` | carries the `bind` for the *whole group* (one field, e.g. `mode`); children are `radiobutton`s with a static `value` matching the bound field's type, not their own bind. This is a real asymmetry vs. `checkbox`, not an inconsistency — a checkbox is one independent bool-like field, a radio group is one field with N possible values. |
| `navlist` | not a distinct render kind — it's a `column` styled as a menu, whose children are ordinary `button`s. Exists only as a naming/style hint. |

Leaves:

| kind | bind shape | notes |
|---|---|---|
| `label` | `string`, or static `text` | |
| `header` | `string`, or static `text` | same bind shape as `label`, section-title render |
| `button` | unbound | `on_press: <action name>` and/or `navigate: <screen name>` (see nav + action-dispatch sections below) |
| `image` | `string` (asset key), or static `asset` | binds to an asset key, not raw bytes — matches the doc's pre-rotated-asset model |
| `progress` | numeric + `range: {min, max}` | linear bar |
| `gauge` | numeric + `range: {min, max}` | arc/dial; identical bind shape to `progress`, different render only |
| `checkbox` | `int` (0/nonzero convention) | bound per-widget. **harpia has no `bool` type** (confirmed against `LexicalAnalizer/LexicalAnalyzer.py` — only `int`/`int64`/`float`/`string`/`map`), so this is a deliberate mapping, not an oversight |
| `radiobutton` | static `value` only | bind lives on the parent `radiogroup` |
| `led` | `int` (state index) | display-only, not interactive; optional static `states: [off, on, warn, ...]` maps int→color, same spirit as `progress`'s `range` |
| `divider` | unbound | purely decorative — a thin rule/spacer; no runtime state, no draw content beyond a fixed fill |
| `toggle` | `int` (0/nonzero convention) | identical bind shape to `checkbox` — a switch-styled render of the same on/off value |
| `badge` | `int` (0/nonzero convention) | a small on/off status dot — same bind shape as `checkbox`/`toggle`, distinct fill so it reads as its own kind |
| `slider` | numeric + `range: {min, max}` | identical bind shape to `progress`/`gauge` — display-only in v1 (shows a live value; doesn't write back). An interactive, write-back slider is a separate, larger future increment, not this kind |

Project-level nav (not an in-screen widget kind):

| kind | shape | notes |
|---|---|---|
| `tabs` | `targets: [{screen, title}]` in `app.yaml` | switches between **full top-level screens** by name, not sub-panels. Only one screen's widgets are ever live at once — device holds a small `active_screen` index and redraws from that screen's precomputed geometry table on switch, using the same tiled-redraw mechanism as everything else (fits the ~2 KiB transient-buffer budget: no room for two screens' framebuffers at once). |

**Project layout is multi-file**, one screen per file, matching harpia's own multi-Include precedent (`test.harpia` importing `file{1,2,3}.harpia`):

```
app.yaml                    # lists screens + nav
device_status.screen.yaml
settings.screen.yaml
```
```yaml
# app.yaml
screens:
  - device_status.screen.yaml
  - settings.screen.yaml
nav:
  kind: tabs
  targets:
    - { screen: DeviceStatus, title: "Status" }
    - { screen: Settings, title: "Settings" }
```

Worked example exercising every kind above, 2 instances each (`device_status.screen.yaml` covers `label`/`button`/`image`/`progress`/`checkbox`; `settings.screen.yaml` covers the rest):

```yaml
screen: Settings
layout: column
children:
  - { kind: header, id: settings_header, text: "Settings" }
  - kind: header
    id: display_header
    bind: { message: settings, field: display_header_text, type: string }

  - kind: box
    id: network_box
    layout: column
    collapsible: true
    default_expanded: true
    children:
      - kind: row
        children:
          - { kind: led, id: wifi_led, bind: { message: settings, field: wifi_state, type: int }, states: [off, on, warn], size: { w: 10, h: 10 } }
          - { kind: led, id: cloud_led, bind: { message: settings, field: cloud_state, type: int }, states: [off, on, warn], size: { w: 10, h: 10 } }

  - kind: box
    id: sensors_box
    layout: column
    collapsible: true
    default_expanded: false
    children:
      - kind: row
        children:
          - { kind: gauge, id: temp_gauge, bind: { message: settings, field: temperature, type: float }, range: { min: -20, max: 60 }, size: { w: 60, h: 60 } }
          - { kind: gauge, id: humidity_gauge, bind: { message: settings, field: humidity, type: float }, range: { min: 0, max: 100 }, size: { w: 60, h: 60 } }

  - kind: radiogroup
    id: mode_select
    bind: { message: settings, field: mode, type: int }
    layout: row
    children:
      - { kind: radiobutton, id: mode_auto, value: 0, text: "Auto" }
      - { kind: radiobutton, id: mode_manual, value: 1, text: "Manual" }

  - kind: navlist
    id: settings_menu
    layout: column
    children:
      - { kind: button, id: goto_status, text: "Back to Status", navigate: DeviceStatus }
      - { kind: button, id: goto_about, text: "About", on_press: open_about_dialog }
```

→ contributes to `janus_generated.harpia` (alongside `device_status.screen.yaml`'s `message device{...}`):

```
message settings{
    string display_header_text;
    int wifi_state;
    int cloud_state;
    float temperature;
    float humidity;
    int mode;
};
```

## Embedded-C code generation architecture

Verified against the actual harpia generator (not guessed): harpia uses **no templating engine** (no Jinja2/Mako anywhere in the repo). Its real mechanism is `.tmpl` files with `str.format()`-style `{placeholder}` markers (literal C++ braces doubled as `{{`/`}}`), loaded once via `Util/util.py`'s `loadTemplate`. Repeated sections (field lists, FK includes) are built as Python string fragments in adapter helper methods (e.g. `Database/CrudlAdapter.py`'s `_create_locals`/`_extract_set`/`_map_write`) and fed into a single `.format()` call per file. One `.tmpl` + one adapter module per output kind (`Database/`, `JsonAdapter/`, `XmlAdapter/`, `ZmqAdapter/`, each independent, `.tmpl` files living alongside their adapter). Files are written unconditionally (`open(path, "w").write(...)`), and the whole output directory is `shutil.rmtree()`'d before regeneration (`main.py`) — full wipe-and-regenerate, not a smart diff/prune step.

Janus stays consistent with this mechanism (`.tmpl` + `str.format()`, no new templating dependency) but **splits embedded-C output into two things generated very differently**, because Janus's generation surface is lumpier than harpia's (13 widget kinds, `box`'s dual geometry tables, tab/nav switching, driver dispatch — vs. harpia's fairly uniform "one CRUDL class per table"). Templating *all* of that per-project would mean regenerating drawing logic on every build: a rendering bug fix would only land in projects that happen to regenerate, and every generated project would duplicate the same traversal/tiling code. That's exactly the failure mode the Copilot stub never got past.

1. **A fixed runtime library, hand-written once, shipped with Janus, never templated per-project** — `runtime/embedded_c/{include/janus_runtime.h, src/janus_runtime.c}`. This is where DESIGN.md's traversal, tiling, driver contract (`draw_area_sync`/`draw_area_async`/`display_busy`), and one `draw_<kind>()` function per widget kind actually live. Every generated project links the same library; a rendering fix ships by rebuilding, not regenerating. **Adding a new widget kind (e.g. a "bolt-button") means adding one `draw_bolt_button()` function here plus one entry in the Python kind catalog — not touching layout, harpia emission, or any other widget's code.** (Caveat: a kind that needs a *new bind shape* — like `radiogroup`'s group-level bind — or *new runtime state* — like `box`'s collapse bit — touches a few more places: the shared descriptor/state struct and the layout pass if it affects geometry. Still additive, never a rewrite.)

2. **Per-project generated *data*, not logic** — for each screen, a small C array of widget descriptors + geometry (two tables for a collapsible `box`) + the screen/nav table, harpia-style-templated exactly like `CrudlAdapter` does it:

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
   }};
   ```

   `{widget_entries}` is built by a Python loop over the IR (one initializer line per widget), joined and dropped into the single `.format()` call — the exact same two-tier pattern as `crudl.h.tmpl` + `CrudlAdapter._create_locals`, just applied to a much smaller surface (data, not control flow).

3. **Deliberate improvement over harpia, not a borrowed one:** since generated `.c` files get compiled, an unconditional overwrite touches mtime and forces recompilation even when content is byte-identical — wasteful for embedded incremental builds. Janus's own file-writer should do a real diff-before-write (compare content, skip the write if unchanged).

**RESOLVED (2026-08-19), corrects the "Open question" below:** a widget descriptor's `.bind.field_offset = offsetof(device_t, name)` assumes a real C struct `device_t` exists with that memory layout. Two research passes the same day confirmed harpia can never be the source of it — `ZmqAdapter` only emits header-only C++ wrapping protobuf's own C++ classes, and one level deeper, `protoFile/ProtoCompiler.py` hardcodes `protoc --cpp_out` with no `--c_out`/nanopb/protobuf-c path anywhere in harpia. Structurally, no plain C struct exists in that pipeline for any message. So Janus generates its own: `emit_bindings_struct.py` (Stage 3b) walks the same `Binding`s `emit_harpia.py` already collects and emits `{message}_t` + a zero-initialized `{message}_instance` — `janus_bindings.gen.h`/`.gen.c`, replacing the hand-written `examples/host_demo/janus_bindings.h`/`.c` this doc originally described. See `architecture.md`'s Stage 3b/7 for the exact contract; the paragraph below is kept for the record of what was actually unresolved before this pass, not because it's still accurate.

**Open question as it stood before the above:** a widget descriptor's `.bind.field_offset = offsetof(device_t, name)` assumes a real C struct `device_t` exists with that memory layout — and *what generates that struct* was unchecked. harpia's own C++ backend (protobuf/SOCI) is explicitly too heavy for this target (see "Why this isn't part of the harpia repo" above); the plan was harpia's `ZmqAdapter` for transport, but whether `ZmqAdapter` emits anything usable as a plain C struct (vs. C++-only protobuf classes) hadn't been checked. This was a real dependency for the doc's own step 2 ("DSL → harpia Include + working UI code for a trivial one-screen example") — needed a research pass into `harpia/ZmqAdapter/` before real generator code got written, not something to guess at.

## Input / event dispatch (deferred, sketched — not yet designed in detail or implemented)

Everything above is the *output/rendering* half. Input (how a physical touch, encoder turn, or push button actually triggers a widget's `on_press`/`navigate`) is a separate, currently entirely unbuilt layer — same status as real widget rendering in the original doc ("Not solved yet"). Sketch of how it should fit the existing architecture, so it isn't lost:

- **Input modality is pluggable, not a rewrite, because widget geometry and IDs already live in the generated descriptor table.** Touch input is "hit-test a point against the geometry rects already in `screen_table.gen.c`, find the widget, dispatch its action" — one function using data that already exists. Encoder input instead needs a *focus order* (which widgets are focusable, in what sequence a rotation moves between them) — a genuinely new, currently-unmodeled piece of per-widget data, but additive: an optional `focus_order` field in the generated descriptor, plus a new `janus_input_encoder.c` module in the runtime library, separate from `janus_input_touch.c`. Physical push buttons are a third, likely even simpler modality (discrete GPIO edges mapped directly to an action or a focus-move, no relative movement to track). All three share the same downstream action-dispatch step — only the "which widget got selected" front-end differs per modality. None of the three exist yet; none is required before the others. **Physical push-button navigation does not need to be implemented now** — deferring it costs nothing, since no input modality is built yet and the architecture already has a slot for it as another pluggable module later.
- **Wiring a button to a user-defined action reuses the schema-ownership split already established for harpia Includes**, applied to actions: Janus generates a stable, cheap-to-regenerate **action ID enum** (one entry per distinct `on_press`/`navigate` value across all screens, e.g. `JANUS_ACTION_REBOOT`), and each widget descriptor carries `.action = JANUS_ACTION_REBOOT` (an int, not a string). The human writes **one** hand-maintained, never-regenerated function — `void janus_handle_action(janus_action_t action)` — with one `case` per action, exactly like the human's root `.harpia` file: Janus owns the enum (regenerated in full every time, trivial since it's just names), the human owns the dispatch body. Adding a new action-producing widget means the enum picks up one new value and the human adds one `case` — not a regeneration of "the entire action set," and never touches the human's existing cases.

## Open questions (Stage 1 scoping — not yet decided)

- **Read access to hand-written harpia files.** Currently out of scope:
  Stage 1 assumes every bound field lives in the Janus-generated Include.
  Binding a widget to a field the human declared themselves (not generated
  by Janus) would need Janus to parse existing harpia files read-only —
  deferred, not required for a first working version.
- **Auto-sizing widgets (deferred, anticipated for i18n).** v1 requires
  explicit `size: {w, h}` on leaf widgets; Janus computes position only, not
  size, at its build-time layout pass. No translations exist yet, but when
  they arrive, fixed pixel widths will break for locales with longer text.
  The fix should still fit the no-malloc/~2 KiB runtime budget: do
  auto-sizing at Janus's *generation* time (one geometry table baked per
  locale, using that locale's glyph atlas metrics), not at device draw time.
  Not needed until font/glyph packing (already "Stage 2+", unbuilt) exists.
- **Janus's own toolchain — no longer open.** Python, confirmed in use
  throughout. Test strategy: `unittest` for the Python pipeline (Stages
  1–3b, 5, 8; 83+ tests), CMake/`ctest` for the C runtime (Stage 4/6).
  Repo layout: `janus/stage{N}_*/` subpackages matching architecture.md's
  own stage numbering.

## Salvageable from the Copilot GuiAdapter branch

Still present, in full, on harpia's `dev` branch (`GuiAdapter/` — see the
correction above) — this section was written under the earlier, wrong
assumption that it was gone. Worth reading directly rather than relying
on the summary below if `DESIGN.md`'s exact wording ever matters.

- `GuiAdapter/DESIGN.md`'s runtime contract, memory model, and tiling
  design — reusable close to as-is as the starting spec for Janus's
  embedded-C target.
- `GuiAdapter/runtime/gui_runtime.{c,h}` — reusable as a rough
  skeleton/reference for API shape only; the actual rendering logic was a
  non-functional stub and needs to be written for real.
- `GuiAdapter/tool/generator.py` — not worth carrying forward: ~90 lines of
  throwaway struct-emission code with no error handling, no schema
  derivation, and no connection to any real input/output contract Janus
  needs.
- The YAML input format and the `GuiAdapter/` naming/location themselves:
  discard both — that's precisely the "looks integrated, isn't" problem
  this project exists to avoid repeating.

## Suggested next steps for whoever picks this up

1. Decide the concrete screen/widget DSL syntax (or explicitly adopt YAML
   as a v1 placeholder and say so).
2. Pick the first target (embedded C or JS/Node) and scope just that one
   end-to-end: DSL → harpia Include + working UI code for a trivial
   one-screen, one-bound-field example.
3. Stand up Janus's own repo scaffolding (language/toolchain, tests, CI) —
   none of this exists yet.
4. Only after a first target works end-to-end, revisit the "read access to
   hand-written harpia files" and second-target questions above.
