# desktop_demo

Janus's desktop (SDL2) target, end to end: the same `app.yaml` as
[`../host_demo`](../host_demo) (encoder input, three screens, nav tabs)
rendered in a real window on Linux/Windows.

Needs CMake and SDL2 development files (`libsdl2-dev` on Debian/Ubuntu).

```sh
./generate.sh                                   # spec -> build/generated + src/ scaffolds
cmake -S src -B build/app -DJANUS_GENERATED_DIR=../build/generated
cmake --build build/app
./build/app/janus_desktop_app
```

Keys (encoder modality): **Left / Right** move focus, **Enter** activates.
Close the window to quit.

`src/` is yours: `generate.sh` never overwrites `main.c`, `janus_actions.c`,
`desktop_input.c` (the key mapping) or `CMakeLists.txt` once they exist.
Everything under `build/generated/` is regenerated every run.

No display? `SDL_VIDEODRIVER=dummy ./build/app/janus_desktop_app` runs
headless (that is how `tests/test_desktop_demo.sh` checks it).

## Mirror mode

To show what a *physical device* shows instead (PC input never changes the
screen), generate with `--mirror` — see `architecture.md`, Stage 8 "Mirror
mode". The demo itself stays the interactive one.
