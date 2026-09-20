# Task 4: janus-sh-multi-target

## Contract

`scripts/janus.sh` installs **several targets from one run**, each into
its own destination. Found 2026-09-20 (Rafael, after testing the desktop
target): with one `--target` per run, three targets means three full
pipeline runs when `generate()` already produces every target in one.
Pre-beta, so no backward compatibility with task 2's positional
`<dest-dir>` form.

### Delivered

- **New usage:**
  `scripts/janus.sh <app.yaml> --target <name> <dest-dir> [--target <name> <dest-dir> ...] [--scaffold-src DIR]`
  - `--target` takes **two** values (name, dest) and may repeat; at least
    one is required. No positional `<dest-dir>` anymore.
  - Duplicate target names are a usage error.
  - **Validate before installing:** if any requested target has no
    generator yet (e.g. `android`), fail loudly ("target '<name>' has no
    generator yet") before anything is written to *any* destination.
  - One pipeline run (`python -m janus.cli`) regardless of target count.
- **`--scaffold-src DIR`:** with exactly one `--target`, scaffold files
  land flat in `DIR/` (as today). With more than one, each target's land
  in `DIR/<target>/` — `main.c`/`janus_actions.c` differ per target and
  would otherwise collide. Never clobbers an existing file (unchanged).
- Install discipline unchanged: content-diffed, no-clobber scaffolds.
- `examples/host_demo/generate.sh`, `tests/test_janus_sh.sh`, the
  script's header comment and any doc naming the old form updated.

### Not in scope

- Any change to `janus/cli.py`/`generate()` — it already writes every
  target; only the shell installer changes.
- Consumer repos' own scripts (ArduinoIHM etc.).

## Dependencies

Task 2 (the `--target` installer this extends) — delivered.

## Tests

`tests/test_janus_sh.sh`: existing cases ported to the new syntax, plus
(g) two targets in one run land in two destinations, (h) an unimplemented
target among valid ones aborts with nothing installed anywhere, (i)
multi-target `--scaffold-src` uses `DIR/<target>/`, (j) duplicate target
is a usage error.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`tests/test_janus_sh.sh` green · `ctest` on a generated desktop tree
green · `scripts/avr_gate.sh` green · docs updated · this file `git mv`-ed
to `4-janus-sh-multi-target-done.md` · epic table updated · commit +
merge `4-janus-sh-multi-target → tasks`.
