# Task 3: optional-desktop-driver

## Contract

`runtime/desktop/CMakeLists.txt` builds without SDL2 when the driver is
switched off.

### Delivered

- `option(JANUS_BUILD_DESKTOP_DRIVER "Build the SDL2 driver (needs SDL2)" ON)`.
  `find_package(SDL2 REQUIRED)`, `janus_desktop_driver`, and
  `janus_desktop_driver_tests` (+ its `add_test`) only when ON; default
  behavior identical to today.
- Test: configure the vendored desktop runtime with
  `-DJANUS_BUILD_DESKTOP_DRIVER=OFF` and SDL2 hidden
  (`-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=ON`), build, `ctest` passes
  (11 suites: everything but the driver's).
- Docs: `architecture.md` desktop driver paragraph notes the switch.

## Dependencies

Task 1 (same file's flag lines — sequential to avoid conflicts).

## DoD

As task 1.
