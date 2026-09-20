# Epic: multi_target_pipeline

Restructure Janus's own generation pipeline so it can hand out more than
one target's output from a single run, before any second target exists.
`embedded_c` is the only real target this epic ships — every byte it
produces stays identical, just relocated — but the registry, directory
shape, and `scripts/janus.sh` interface it establishes are what
`desktop_sdl2_runtime`/`desktop_scaffold` plug into next.

## Motivation

Today `janus/cli.py`'s `generate()` and `janus/generate.py`'s
`write_project()` know exactly one target: they write straight into
`target_dir/{src,include,runtime}` and `scaffold_src/{main.c,
janus_actions.c}`, with no notion that a second target could ever exist.
Adding `desktop` without this step would mean hardcoding an `if target ==
...` branch through both files — worth doing properly once, especially
since `android` is coming after it.

## Decisions (settled with Rafael 2026-09-19 — fixed constraints for the tasks below)

1. **Janus generates every known target on every run; it does not accept
   a `--target` flag itself.** `generate()` loops over a small registry
   (`embedded_c` today; `desktop`/`android` added when their own epics
   ship) and writes each into its own subtree. Python never selects —
   selection is a shell-script-layer concern (decision 3).
2. **Nested-by-target output, uniformly — `embedded_c` included.**
   `target_dir/<target>/{src,include,runtime}` and
   `target_dir/<target>/janus_generated.harpia`; scaffold output becomes
   `scaffold_src/<target>/{main.c,janus_actions.c}`. The `.gen.c`/`.gen.h`
   /`.harpia` content `write_project` produces is actually
   target-agnostic (baked rects/colors/text, nothing OS-specific) and
   will be byte-identical across every target's copy — duplicating it is
   deliberate, for one uniform shape every target directory shares
   rather than special-casing the one part that happens to be shared
   today. Files are tiny; this is not worth optimizing away.
3. **`scripts/janus.sh` is the only sanctioned way to run Janus** — this
   repo's own script, already the documented entry point
   (`GeneralMedicalDevices/device-to-harpia-workflow.md` already treats
   *some* wrapper script as the black-box boundary, and it lives here,
   not in a consumer repo, per Rafael 2026-09-19). It gains a required
   `--target <name>` flag: internally it generates into a throwaway temp
   directory (`python -m janus.cli <app.yaml> <tmp> --scaffold-src
   <tmp>/_scaffold`), then copies only `<tmp>/<name>/*` flat into the
   caller's `<dest-dir>` (and `<tmp>/_scaffold/<name>/*` into
   `--scaffold-src DIR` if given) — the exact flat shape every existing
   consumer already expects. Calling `python -m janus.cli` directly is no
   longer a supported consumer path (Janus's own test suite may still
   address any target's subtree directly — it isn't "a consumer").
4. **The install copy never clobbers an existing scaffold file** — same
   "written once, then human-owned" guarantee `write_if_missing` already
   gives `main.c`/`janus_actions.c` on the Python side, now also
   enforced across the temp-dir-to-dest-dir copy step in the shell
   script.
5. **`android` is a name, not code.** The registry has a slot for it and
   `<tmp>/android/` is a directory nothing currently populates.
   `scripts/janus.sh --target android` must fail with a clear message
   ("target 'android' has no generator yet") rather than silently
   producing an empty directory or crashing with a stack trace.
6. **`examples/host_demo` regenerates itself the same way a real consumer
   would.** Its current `generate.py` calls `janus.cli.main()` directly
   in-process — a path no actual consumer uses or, after decision 3, can
   use. Task 3 replaces it with `generate.sh`, shelling out to
   `scripts/janus.sh --target embedded_c` — `host_demo` is Janus's own
   proof that the documented, sanctioned path actually works, so it has
   to exercise that path, not a shortcut around it.

## Tasks

| # | file | status | contract (one line) |
|---|---|---|---|
| 1 | `tasks/1-target-registry-and-pipeline-done.md` | ✅ done | A small `TARGETS` registry drives `write_project`/`generate()`; `embedded_c` moves through it into `target_dir/embedded_c/...` + `scaffold_src/embedded_c/...`, byte-identical content, nothing else changes. |
| 2 | `tasks/2-janus-sh-target-install-done.md` | ✅ done | `scripts/janus.sh` gains `--target <name>`: generates into a temp dir, installs the selected target flat into the caller's `dest-dir`/`--scaffold-src`, no-clobber on scaffold files, clear error for an unimplemented target. |
| 3 | `tasks/3-host-demo-path-update.md` | not started | `examples/host_demo`'s `generate.py` (which calls `janus.cli.main()` in-process — no real consumer can do that once task 2 lands) replaced by `generate.sh`, calling `scripts/janus.sh --target embedded_c` like any real consumer must; `CMakeLists.txt` updated for the resulting flat paths. |

## Acceptance gate

1. `examples/host_demo` builds and its `ctest`/Python suites stay green,
   proving `embedded_c`'s generated output is unchanged in content
   (only its on-disk location moves).
2. `scripts/janus.sh <app.yaml> <dest> --target embedded_c [--scaffold-src DIR]`
   reproduces file-for-file what today's
   `python -m janus.cli <app.yaml> <dest> --scaffold-src DIR` produces.
3. `--target desktop` and `--target android` both fail with the clear,
   documented error from decision 5 — no silent no-op, no traceback.

## Out of scope

- The `desktop` target's actual runtime/scaffold content (next two
  epics — `desktop_sdl2_runtime`, `desktop_scaffold`).
- Updating any consumer repo's own script
  (`GeneralMedicalDevices/scripts/janus-generate.sh`, whatever ArduinoIHM
  uses) to call the new `scripts/janus.sh --target ...` interface —
  flagged as a follow-up in those repos, not part of this work.
