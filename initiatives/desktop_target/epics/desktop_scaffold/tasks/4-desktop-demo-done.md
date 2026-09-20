# Task 4: desktop-demo

## Contract

`examples/desktop_demo`: proof for the epic and the initiative.

### Delivered

- `examples/desktop_demo/generate.sh`: one `scripts/janus.sh` call on
  `../host_demo/app.yaml` for `--target desktop` (also exercises the
  multi-target form with `embedded_c` into a scratch dir if cheap);
  scaffolds into `examples/desktop_demo/src/`; scaffolded files
  committed (human-owned), generated output in gitignored `build/`.
- `examples/desktop_demo/README.md` (short: generate, cmake, run, keys).
- A shell test (`tests/test_desktop_demo.sh`): generate, configure with
  `-DJANUS_GENERATED_DIR`, build, run under `SDL_VIDEODRIVER=dummy`
  (alive after N seconds; clean exit on `SDL_QUIT`).
- Docs: `architecture.md`/`Janus.md` mention; initiative cross-epic gate
  checked and recorded.

### Not in scope

Visual verification (Rafael, on his machine).

## Dependencies

Tasks 1–3.

## DoD

Epic acceptance gate (1–3) passes · file renamed `-done` · epic table
updated · commit + merge `→ tasks`; epic merge-up per branch rules.
