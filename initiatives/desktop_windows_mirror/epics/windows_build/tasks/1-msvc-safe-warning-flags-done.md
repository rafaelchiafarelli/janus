# Task 1: msvc-safe-warning-flags

## Contract

No CMakeLists or template hard-codes GCC-only warning flags.

### Delivered

- `set(JANUS_WARN_FLAGS $<$<C_COMPILER_ID:MSVC>:/W3>
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wextra>)`
  near the top of `runtime/embedded_c/CMakeLists.txt`,
  `runtime/desktop/CMakeLists.txt`,
  `janus/templates/desktop_app_CMakeLists.txt.tmpl`,
  `examples/host_demo/CMakeLists.txt`, and
  `examples/desktop_demo/src/CMakeLists.txt`; every
  `target_compile_options(<t> PRIVATE -Wall -Wextra)` becomes
  `... PRIVATE ${JANUS_WARN_FLAGS})`.
- A Python test that fails if any `CMakeLists.txt`/`.tmpl` under the repo
  (outside `.venv`/`build`) still contains a literal `-Wall -Wextra`
  outside the one variable definition.

## Dependencies

None.

## DoD

Contract delivered · Python suite green · `ctest` on generated
embedded_c *and* desktop trees green · `avr_gate.sh` green ·
`test_desktop_demo.sh` green · file renamed `-done` · epic table updated
· commit + merge `→ tasks`.
