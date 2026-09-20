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

- **`examples/host_demo/generate.py` deleted**, replaced by
  **`examples/host_demo/generate.sh`**: a small bash script calling
  `"$JANUS_ROOT/scripts/janus.sh" "$HERE/app.yaml" "$HERE/build/generated" --target embedded_c --scaffold-src "$HERE/src"`.
  Same inputs/outputs as the old `generate.py` — only the mechanism
  changes.
- **No `git mv` needed, and no `CMakeLists.txt` changes needed** —
  both were sketched during planning before task 2 existed and turned
  out to be based on the wrong invocation. Task 2's `scripts/janus.sh`
  already strips the `embedded_c/` nesting before anything reaches the
  caller's `dest-dir`/`--scaffold-src`, so `build/generated/src/*.gen.c`
  and `src/{main.c,janus_actions.c}` land at exactly the same flat paths
  they always have. Verified directly: `examples/host_demo`'s own
  `CMakeLists.txt`, completely untouched, configures and builds both
  `host_demo` and `host_demo_firmware_main` clean against `generate.sh`'s
  output, and the pre-existing hand-edited `src/main.c`/`janus_actions.c`
  are left alone (no-clobber) exactly as before.
- **`scripts/avr_gate.sh`** updated: its `GEN` path (which task 1 had
  pointed at `.../embedded_c` — correct for the *old* `generate.py` call
  it made directly, now wrong) reverted to the plain
  `build/generated`, and its regeneration call switched from
  `"$PY" "$HOST_DEMO/generate.py"` to `"$HOST_DEMO/generate.sh"`.
- **Found while verifying, not a regression:** `avr_gate.sh`'s avr-size
  numbers changed (35500→35590 bytes program, 1568→1588 data) between
  the pre-task-3 and post-task-3 state, confirmed via a side-by-side
  `git worktree` comparison with byte-identical `app.yaml`/`*.screen.yaml`
  /assets on both sides. Root cause: `scripts/janus.sh` has always
  preferred `$REPO_ROOT/.venv/bin/python` when present (predates this
  epic); the old `generate.py`, invoked by `avr_gate.sh` as plain
  `python3`, never got that preference. This repo's `.venv` has Pillow
  installed, system `python3` does not — so the `pwm_icon` image widget
  in `host_demo`'s own `pwm.screen.yaml` was silently baked without its
  real pixel data (`pwm_str1_px[]` absent entirely, not just a
  placeholder) every time `generate.py`/`avr_gate.sh` ran, and only now,
  routed through `scripts/janus.sh` for the first time, gets decoded for
  real. `avr_gate.sh` has no hardcoded byte-count assertion, only the
  budget/symbol-placement checks (still pass) — this is a correctness
  improvement task 3 incidentally surfaced, not something to chase
  further here.
- **Docs**: no other doc named `examples/host_demo/generate.py`
  specifically (checked `Janus.md`, `architecture.md` — their
  `janus-generate`/`--scaffold-src` mentions describe `janus.cli`'s own
  flags, unrelated to this file).

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
