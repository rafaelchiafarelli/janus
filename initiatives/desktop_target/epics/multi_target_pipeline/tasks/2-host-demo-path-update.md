# Task 2: host-demo-path-update

## Contract

`examples/host_demo` — Janus's own committed, built, and `ctest`-verified
consumer of its generated output — is updated for task 1's nested-by-
target layout, proving in a real build (not just unit tests) that
`embedded_c`'s generated content is unchanged, only relocated.

### In

- Task 1's registry-driven `generate()`, writing `embedded_c` output
  under `.../embedded_c/{src,include,runtime}` wherever `host_demo`
  regenerates it.

### Delivered

- **`examples/host_demo/CMakeLists.txt`**: every path currently reading
  `build/generated/src/*.gen.c`, `build/generated/include`, and
  `${RUNTIME_DIR}` (the vendored-in-place `runtime/embedded_c` used
  directly today, unrelated to this nesting but worth confirming still
  resolves) gets the `embedded_c/` segment added wherever `host_demo`'s
  own regeneration step (however it currently invokes Janus — check
  whether it calls `scripts/janus.sh` or `python -m janus.cli` directly
  today, since task 3 changes the former's contract) now nests its
  output.
- Any other fixture or doc under `examples/host_demo/` that hardcodes
  the old flat path (its own `README.md`, if it documents the generate
  command or output layout).

### Not in scope

- `scripts/janus.sh` itself (task 3 — this task may temporarily keep
  calling `python -m janus.cli` directly if `host_demo`'s regen step
  already does today; wire it to `scripts/janus.sh --target embedded_c`
  as part of task 3 instead of duplicating that change here).

## Dependencies

Task 1 delivered and merged (registry-driven, nested `embedded_c` output
exists to point `host_demo` at).

## Tests

- `host_demo`'s own build (`ctest` target and/or the CMake targets
  `architecture.md`/task 1 reference) succeeds against the new paths.
- Full `ctest` suite (`runtime/embedded_c/build`) stays green — this task
  should change zero runtime behavior, only where `host_demo` looks for
  its generated sources.
- `python -m unittest discover -s tests` stays green.

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · docs updated · this file
`git mv`-ed to `2-host-demo-path-update-done.md` · commit + merge
`2-host-demo-path-update → tasks`.
