# Task 1: target-registry-and-pipeline

## Contract

A small target registry drives Janus's generation orchestration
(`janus/cli.py`'s `generate()`), which now loops over every registered
target and writes each into its own subtree instead of hardcoding
`embedded_c`. `embedded_c`'s own generated content does not change one
byte — only where it lands on disk does.

### In

- Everything `generate()` already receives: a parsed `App` IR, a
  `target_dir`, an optional `scaffold_src`.

### Delivered

- **`janus/targets.py`** (new): a `TARGETS` registry — a small dict/
  dataclass keyed by target name (`"embedded_c"` populated;
  `"desktop"`/`"android"` reserved keys with no entry yet, or an entry
  that `generate()` recognizes as "known but unimplemented" — whichever
  reads cleaner once written) carrying what `janus/cli.py`'s
  `_vendor_runtime()` currently hardcodes for `embedded_c`:
  `_RUNTIME_EMBEDDED_C` (the source dir to vendor from),
  `_VENDORED_SUBDIRS`, `_VENDORED_ROOT_FILES`.
- **`janus/cli.py`**: `generate()` no longer writes straight into
  `target_dir`/`scaffold_src` — for each known target it calls
  `write_project(app, target_dir / target_name)` (unchanged function,
  just called once per target — `write_project`'s own output is
  target-agnostic baked data, see epic decision 2) and, in scaffold mode,
  scaffolds into `scaffold_src / target_name` and vendors that target's
  runtime into `target_dir / target_name / "runtime"`. `_vendor_runtime()`
  takes its source/subdirs/root-files from the registry entry instead of
  the current module-level constants.
- **`janus/stage8_scaffold/scaffold_main.py` / `janus/stage5_actions/scaffold_actions.py`:
  unchanged, turned out not to need target-awareness.** Only one target
  is ever implemented at a time in the registry today, so `cli.py`
  calling `scaffold_main_c(app, target_scaffold / "main.c")` once per
  implemented target (with a different path each time) is sufficient —
  no template-map restructuring needed until `desktop_scaffold` actually
  has a second template set to choose between. Simpler than sketched
  during planning; revisit target-awareness there when that epic starts.
- **`janus/generate.py`**: unchanged. `write_project(app, out_dir)` never
  knew about targets and still doesn't — `generate()` just calls it once
  per target with a different `out_dir`.
- **`scripts/avr_gate.sh`**: its `GEN` path (`$HOST_DEMO/build/generated`)
  updated to `$HOST_DEMO/build/generated/embedded_c` — not originally
  scoped to this task, but its glob (`$GEN/src/*.gen.c`) and `-I` flag
  broke immediately once `generate()` started nesting output, and its own
  DoD line ("`scripts/avr_gate.sh` green") can't be satisfied without
  this. Its `generate.py` invocation itself is untouched here — task 3
  still owns replacing that call with `generate.sh`.
- **`tests/test_cli.py`**: updated for the new nested paths (its
  assertions currently read `self.target_dir / "include"` /
  `self.target_dir / "src"` directly — they become
  `self.target_dir / "embedded_c" / "include"` etc.). `tests/
  test_generate.py` needs no change — it tests `write_project` directly,
  which this task does not touch.
- **Docs**: `architecture.md`'s Stage 3b/4/8 sections and this repo's
  own generation-flow description get a short note that output is now
  per-target, embedded_c being the one populated entry.

### Not in scope

- Any `desktop`/`android` registry entry actually producing output
  (next epics).
- `scripts/janus.sh` (task 3) and `examples/host_demo` (task 2).

## Dependencies

None — this is the first task of the first epic of the initiative.

## Tests

- `tests/test_cli.py`: every existing case, paths updated for the
  `embedded_c/` nesting; behavior (which files get written, content-diff
  no-op on a second run, scaffold-once semantics) unchanged.
- New case: `generate()` with the registry containing an unimplemented
  target key raises/logs clearly rather than silently doing nothing for
  it (the registry itself is exercised even though no second target
  exists yet — e.g. a test-only fake target entry, or asserting
  `"desktop"`/`"android"` aren't silently produced as empty directories).

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green (no runtime behavior changed) · `scripts/avr_gate.sh` green
· docs updated · this file `git mv`-ed to
`1-target-registry-and-pipeline-done.md` · commit + merge
`1-target-registry-and-pipeline → tasks`.
