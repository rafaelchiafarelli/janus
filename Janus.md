# Janus — a pre-harpia GUI schema + code generator

> **Status: design only.** Nothing in this doc is implemented yet. This file
> is meant to let a fresh session pick this project up and move quickly —
> read it whole before writing any code.

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
attempt lived on harpia's `gui-dsl-prototype` branch (`GuiAdapter/`) — now
deleted, both locally and on `origin`, after the analysis below. harpia's
`NEXT_SESSION.md` has a one-line pointer back to this file.

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
  Same model harpia's own generated C++ output already uses
  (`Util.util.write_if_different`/`prune_stale_outputs`).
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

## Open questions (Stage 1 scoping — not yet decided)

- **Concrete DSL syntax.** Copilot's prototype used arbitrary YAML
  (`screen: {widgets: [...]}`), which is a reasonable starting point for
  velocity but hasn't been reviewed or committed to as Janus's real syntax.
- **Read access to hand-written harpia files.** Currently out of scope:
  Stage 1 assumes every bound field lives in the Janus-generated Include.
  Binding a widget to a field the human declared themselves (not generated
  by Janus) would need Janus to parse existing harpia files read-only —
  deferred, not required for a first working version.
- **Which target ships first** — embedded C or JS/Node. Not decided. The
  two targets are independent enough (different runtime, different
  transport) that either can go first without blocking the other.
- **Janus's own toolchain.** Python (for consistency with harpia's own
  toolchain) is the default assumption, not a confirmed decision. Test
  strategy, repo layout, and CI are all unstarted.

## Salvageable from the deleted Copilot branch

The branch itself is gone (deleted from harpia both locally and on
`origin`), so treat this as a memory of what was in it, not a pointer to
live code.

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
