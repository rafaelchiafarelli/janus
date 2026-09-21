"""Janus's CLI: Stage 1 -> 2 -> 3a -> 3b in one command. Parses an
app.yaml (Stage 0's origin file), runs the layout pass, and writes the
generated harpia Include + embedded-C files to a target directory —
content-diffed, same as `generate.write_project` on its own. See
architecture.md for what each stage produces.

Installed as the `janus-generate` console script (pyproject.toml); also
runnable directly as `python3 -m janus.cli`.
"""
from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

from .generate import write_project
from .stage1_parse.dsl_yaml import parse_app
from .stage2_layout.layout import build_nav_bar, build_status_bar, check_fits_display, layout_screen
from .stage3b_embedded_c.emit_files import render_render_config_header
from .stage5_actions.scaffold_actions import scaffold_actions_c
from .stage8_scaffold.scaffold_cmake import scaffold_desktop_cmake
from .stage8_scaffold.scaffold_input import scaffold_desktop_input_c, scaffold_mirror_link_c
from .stage8_scaffold.scaffold_main import scaffold_main_c
from .targets import Target, implemented_targets
from .writer import copy_tree_if_changed, write_if_changed


def _vendor_runtime(
    target: Target, dest_dir: Path, render_mode: str, display=None, has_nav: bool = False,
    has_status: bool = False,
) -> list[Path]:
    written = []
    # Root files (unlike subdirs) can legitimately exist under more than
    # one vendor_from root with different content — e.g. desktop's own
    # CMakeLists.txt overwriting embedded_c's shared one. Resolve which
    # root actually wins *before* writing, so each file is written at
    # most once per run: writing every root's version in sequence would
    # never reach a stable no-op (each run's first write would differ
    # from the previous run's final content).
    for fname in target.vendored_root_files:
        src = None
        for root in target.vendor_from:
            candidate = root / fname
            if candidate.is_file():
                src = candidate
        if src is not None and write_if_changed(dest_dir / fname, src.read_text()):
            written.append(dest_dir / fname)
    for subdir in target.vendored_subdirs:
        for root in target.vendor_from:
            src_subdir = root / subdir
            if src_subdir.is_dir():
                written.extend(copy_tree_if_changed(src_subdir, dest_dir / subdir))

    # The one generated file that lands *inside* the vendored fixed
    # library: janus_runtime.{c,h} pull it in via __has_include so a
    # `blocking` project compiles out the polled/async render path (and
    # its 6912-byte g_async_ops buffer — channel_icons task 3). Written
    # for `blocking` too, macro undefined, so the include never dangles.
    cfg = dest_dir / "include" / "janus_render_config.gen.h"
    if write_if_changed(cfg, render_render_config_header(render_mode, display, has_nav, has_status)):
        written.append(cfg)
    return written


def generate(
    app_yaml: str | Path,
    target_dir: str | Path,
    scaffold_src: str | Path | None = None,
    mirror: bool = False,
) -> list[Path]:
    """Runs the pipeline against `app_yaml` and writes every implemented
    target (`janus/targets.py`) into its own subtree of `target_dir` —
    `target_dir/embedded_c/...` today, `.c` under `.../src`, `.h` under
    `.../include` (see `write_project`) — never straight into
    `target_dir` itself; a reserved-but-unimplemented target (`desktop`,
    `android`) is skipped entirely, not written as an empty directory.
    If `scaffold_src` is given, this is scaffold mode: each target's
    `main.c`/`janus_actions.c` get scaffolded under
    `scaffold_src/<target>/` the first time they're needed (Stage 5/8,
    never touched again once they exist), and that target's fixed
    runtime library is vendored into `target_dir/<target>/runtime`
    (content-diffed, same discipline as every other Janus output) so the
    project can build against its own copy instead of reaching into the
    Janus checkout. Without `scaffold_src`, neither happens — `target_dir`
    holds only the regenerated-every-run Janus output. Returns every path
    actually written (empty on a no-op re-run).

    `mirror=True` (desktop target only; `janus.sh --mirror`) scaffolds the
    input-less mirror `main.c` plus a transport-hook `mirror_link.c` in
    place of `desktop_input.c` — the window then shows a device's UI
    state (janus_remote.h) instead of taking local input. Every other
    target is unaffected.

    Janus itself never selects a target — every implemented one is always
    written. `scripts/janus.sh --target <name>` is what a real consumer
    uses to install just the one it wants, flat, into its own project."""
    app = parse_app(app_yaml)
    has_nav = app.nav is not None
    has_status = app.status is not None
    for screen in app.screens:
        layout_screen(screen, app.display, has_nav=has_nav, has_status=has_status)
        if app.display is not None:
            check_fits_display(screen, app.display)
    app.nav_bar = build_nav_bar(app)
    app.status_bar = build_status_bar(app)

    target_dir = Path(target_dir)
    scaffold_src = Path(scaffold_src) if scaffold_src is not None else None
    render_mode = app.display.render_mode if app.display is not None else "blocking"

    written: list[Path] = []
    for target in implemented_targets():
        target_out = target_dir / target.name
        written.extend(write_project(app, target_out))

        if scaffold_src is not None:
            target_scaffold = scaffold_src / target.name
            if scaffold_actions_c(app, target_scaffold / "janus_actions.c"):
                written.append(target_scaffold / "janus_actions.c")
            if scaffold_main_c(
                app, target_scaffold / "main.c", target.name, mirror and target.name == "desktop",
            ):
                written.append(target_scaffold / "main.c")
            if target.name == "desktop":
                # The desktop main's input source has no vendor to supply
                # it (there's no board): the default SDL polls — or, in
                # mirror mode, the transport hook that replaces them.
                input_name = "mirror_link.c" if mirror else "desktop_input.c"
                scaffold_input = scaffold_mirror_link_c if mirror else scaffold_desktop_input_c
                if scaffold_input(app, target_scaffold / input_name):
                    written.append(target_scaffold / input_name)
                if scaffold_desktop_cmake(app, target_scaffold / "CMakeLists.txt", mirror):
                    written.append(target_scaffold / "CMakeLists.txt")
            written.extend(
                _vendor_runtime(
                    target, target_out / "runtime", render_mode, app.display, has_nav, has_status,
                )
            )

    return written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="janus-generate",
        description="Run Janus against an app.yaml and write the generated output to a target folder.",
    )
    parser.add_argument("app_yaml", type=Path, help="path to the project's app.yaml (the origin file)")
    parser.add_argument("target_dir", type=Path, help="directory to write generated files into")
    parser.add_argument(
        "--scaffold-src",
        type=Path,
        default=None,
        metavar="DIR",
        help=(
            "scaffold mode: also scaffold src/main.c and src/janus_actions.c "
            "under DIR if they don't exist yet, and vendor the fixed "
            "runtime/embedded_c library into target_dir/runtime"
        ),
    )
    args = parser.parse_args(argv)

    # Generation is "log and keep going" for soft failures (an image
    # asset that won't decode, an oversized bitmap) — surface those on
    # stderr instead of letting them pass silently.
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")

    if not args.app_yaml.is_file():
        parser.error(f"{args.app_yaml}: no such file")

    written = generate(args.app_yaml, args.target_dir, args.scaffold_src)
    for path in written:
        print(f"wrote {path}")
    if not written:
        print("nothing changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
