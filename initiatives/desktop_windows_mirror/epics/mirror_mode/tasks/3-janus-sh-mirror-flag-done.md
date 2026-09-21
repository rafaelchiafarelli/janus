# Task 3: janus-sh-mirror-flag

## Contract

`--mirror` reaches the generator from the command line.

### Delivered

- `janus/cli.py`: `--mirror` → `generate(mirror=True)`.
- `scripts/janus.sh`: `--mirror` (no value) forwarded to `janus.cli`;
  a usage error if no `--target desktop` was requested. Header comment
  documents it.
- `tests/test_janus_sh.sh`: `--mirror` case (desktop tree has
  `mirror_link.c`, no `desktop_input.c`); `--mirror` without desktop is
  rejected with nothing written.
- A test that builds a mirror-scaffolded tree with cmake (only
  `-DJANUS_GENERATED_DIR`), runs it under `SDL_VIDEODRIVER=dummy`, and
  checks it stays alive and exits 0 on SIGTERM.
- Docs: `architecture.md` Stage 8 + `examples/desktop_demo/README.md`
  mention.

## Dependencies

Tasks 1–2.

## DoD

As task 1, plus the epic acceptance gate (1–3); epic merge-up per
branch rules.
