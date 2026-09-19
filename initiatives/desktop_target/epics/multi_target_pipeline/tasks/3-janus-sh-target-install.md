# Task 3: janus-sh-target-install

## Contract

`scripts/janus.sh` becomes the one sanctioned way to run Janus as a
consumer (per epic decision 3): it gains a required `--target <name>`
flag, generates every target into an internal temp directory, and
installs only the caller's chosen target's output flat into the
`dest-dir`/`--scaffold-src` paths the caller gave it — reproducing
exactly the flat shape `python -m janus.cli` used to produce directly,
so no consumer-facing contract changes except adding this one flag.

### In

- Task 1's registry-driven `generate()`, which now always writes every
  known target nested under whatever directory it's given.

### Delivered

- **`scripts/janus.sh`**: new usage
  `scripts/janus.sh <app.yaml> <dest-dir> --target <name> [--scaffold-src DIR]`.
  Behavior:
  1. Creates a throwaway temp directory (`mktemp -d`, cleaned up via
     `trap` on exit).
  2. Runs `python -m janus.cli <app.yaml> <tmp> [--scaffold-src <tmp>/_scaffold]`
     exactly as today, just pointed at the temp dir.
  3. Copies `<tmp>/<name>/*` → `<dest-dir>/` — content-diffed (e.g.
     `rsync -a --checksum` or an equivalent `cp` + comparison) so a no-op
     regeneration touches no mtimes, same discipline `write_if_changed`
     already gives every other Janus output.
  4. If `--scaffold-src DIR` was given, copies `<tmp>/_scaffold/<name>/*`
     → `DIR/` the same way, but **skips any file that already exists** in
     `DIR` (`cp -n` or equivalent) — never clobbers a human-owned
     scaffolded file, matching `write_if_missing`'s guarantee on the
     Python side.
  5. `--target <name>` for a name with no registry entry yet (`desktop`,
     `android`, today) fails loudly: prints "target '<name>' has no
     generator yet" and exits non-zero, no partial copy left behind.
  6. Missing `--target` entirely is a usage error (today's script accepts
     zero required flags beyond the two positionals — this is a
     backward-incompatible change to `scripts/janus.sh` itself, which is
     the point: every consumer must now say which target it wants).
- **`scripts/janus.sh`'s own header comment**: rewritten for the new
  usage, examples updated (the two existing example lines both need
  `--target embedded_c` added).
- **Docs**: any doc in this repo pointing at the old
  `scripts/janus.sh <app.yaml> <target-dir> [--scaffold-src DIR]` usage
  (check `Janus.md`, `architecture.md`, `examples/host_demo/README.md` if
  it exists) updated to the new form.

### Not in scope

- Updating `GeneralMedicalDevices/scripts/janus-generate.sh` or any other
  consumer repo's script to call this new interface — explicit follow-up
  in those repos (epic's "out of scope"), not this task.

## Dependencies

Task 1 delivered and merged (the registry `python -m janus.cli` writes
into the temp dir from). Not dependent on task 2, but task 2's
`host_demo` update should point at this script (`--target embedded_c`)
once this task lands, rather than calling `python -m janus.cli` directly,
if it doesn't already.

## Tests

- A shell-level test (new `tests/test_janus_sh.sh` or folded into
  existing test tooling — whichever this repo's convention favors once
  written) covering: (a) `--target embedded_c` on a fixture `app.yaml`
  reproduces file-for-file what `python -m janus.cli` writes directly to
  an equivalent flat directory; (b) a second run with no spec changes
  copies nothing new (mtimes untouched); (c) `--target desktop` fails
  with the documented message and non-zero exit, no directory left behind
  in `dest-dir`; (d) `--scaffold-src DIR` never overwrites a pre-existing
  `main.c`/`janus_actions.c` in `DIR` even when the temp dir's scaffolded
  version differs.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · the epic's acceptance gate
(items 2–3 in `epic.md`) verified directly · docs updated · this file
`git mv`-ed to `3-janus-sh-target-install-done.md` · commit + merge
`3-janus-sh-target-install → tasks` · `tasks` merges up through
`multi_target_pipeline → epics` (epic's acceptance gate is the last check
before that merge; `desktop_target` stays open for the next two epics).
