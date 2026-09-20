# Task 3: host-demo-path-update

## Contract

`examples/host_demo` — Janus's own committed, built, and `ctest`-verified
consumer of its generated output — regenerates itself the same way any
real consumer now must: by shelling out to `scripts/janus.sh --target
embedded_c`, not by importing `janus.cli` in-process. This is deliberate,
not incidental — `host_demo` exists to prove Janus works the way a user
actually experiences it, and today it doesn't: `generate.py` calls
`janus.cli.main()` directly, a path no real consumer (`GeneralMedicalDevices`,
ArduinoIHM) uses or could use once task 2 makes `scripts/janus.sh` the
only sanctioned entry point.

### In

- Task 2's `scripts/janus.sh --target <name>` (this task only ever
  passes `--target embedded_c`).

### Delivered

- **`git mv examples/host_demo/src/main.c examples/host_demo/src/embedded_c/main.c`
  and the same for `janus_actions.c`, done first, before anything else.**
  These are real, hand-edited files (relay-toggle demo logic, encoder
  focus/nav handling) — not stock templates. Confirmed while testing
  task 1: pointing the new nested scaffold path at the *existing* flat
  `src/` without moving these first silently scaffolds fresh stub
  templates at `src/embedded_c/{main.c,janus_actions.c}` (since
  `write_if_missing` sees no file at that new path) while the real,
  git-tracked content sits orphaned at the old flat path, unreferenced
  by anything. `git mv` first so the same file, same content, just
  changes address — `write_if_missing` then correctly sees it as already
  scaffolded and leaves it alone on every subsequent regen.
- **`examples/host_demo/generate.py` deleted**, replaced by
  **`examples/host_demo/generate.sh`**: a small bash script (matching
  this repo's own convention for these — see `scripts/janus.sh`,
  `scripts/avr_gate.sh`) that calls
  `"$JANUS_ROOT/scripts/janus.sh" "$HERE/app.yaml" "$HERE/build/generated" --target embedded_c --scaffold-src "$HERE/src"`,
  resolving `JANUS_ROOT`/`HERE` the same way `scripts/janus.sh` resolves
  its own repo root. Same inputs/outputs as today's `generate.py`
  (`app.yaml` → `build/generated/` + the Stage 5/8 `src/` scaffolds) —
  only the mechanism changes.
- **`examples/host_demo/CMakeLists.txt`**: paths reading
  `build/generated/src/*.gen.c`, `build/generated/include`, updated for
  the flat shape `scripts/janus.sh --target embedded_c` installs into
  `build/generated/` (no `embedded_c/` segment here — task 2's script
  already stripped that nesting before it reaches `host_demo`).
  `${RUNTIME_DIR}` (pointing directly at `runtime/embedded_c` in the
  Janus checkout, not the vendored copy) is unrelated to this task and
  stays as-is.
- Any place that currently invokes `examples/host_demo/generate.py`
  (a CMake custom command, a CI-style script, or purely manual — check
  before assuming which) updated to call `generate.sh` instead.
  **Known one: `scripts/avr_gate.sh`** regenerates via
  `"$PY" "$HOST_DEMO/generate.py"` directly — task 1 already repointed
  its `GEN` path at `.../embedded_c` (needed for its own DoD to pass),
  but this call site itself still names the file this task deletes; swap
  it for `"$HOST_DEMO/generate.sh"`.
- **Docs**: any doc referencing `examples/host_demo/generate.py` by name
  (`Janus.md`, `architecture.md`) updated to `generate.sh`.

### Not in scope

- `scripts/janus.sh` itself (task 2, already landed by the time this
  task starts).

## Dependencies

Task 2 delivered and merged (`scripts/janus.sh --target` must exist
before `host_demo` can call it).

## Tests

- Running `examples/host_demo/generate.sh` from a clean `build/generated`
  and `src/` reproduces exactly what today's `generate.py` produces
  (content, not just presence — diff against a pre-change baseline
  captured before deleting `generate.py`).
- A second run with no spec changes writes nothing (mtimes untouched),
  proving `scripts/janus.sh`'s content-diffed install (task 2) actually
  holds through a real consumer's use.
- `host_demo`'s own build (`ctest`) succeeds unchanged.
- Full `ctest` suite (`runtime/embedded_c/build`) and
  `python -m unittest discover -s tests` stay green — this task changes
  zero runtime behavior, only how `host_demo` regenerates itself.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · epic acceptance gate
verified end-to-end (a real consumer, `host_demo`, now goes through
`scripts/janus.sh --target embedded_c` exactly like `GeneralMedicalDevices`
or ArduinoIHM would) · docs updated · this file `git mv`-ed to
`3-host-demo-path-update-done.md` · commit + merge
`3-host-demo-path-update → tasks` · `tasks` merges up through
`multi_target_pipeline → epics` (epic's acceptance gate is the last check
before that merge; `desktop_target` stays open for the next two epics).
