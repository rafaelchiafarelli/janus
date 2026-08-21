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
import sys
from pathlib import Path

from .generate import write_project
from .stage1_parse.dsl_yaml import parse_app
from .stage2_layout.layout import check_fits_display, layout_screen
from .stage5_actions.scaffold_actions import scaffold_actions_c
from .stage8_scaffold.scaffold_main import scaffold_main_c


def generate(
    app_yaml: str | Path, target_dir: str | Path, scaffold_src: str | Path | None = None
) -> list[Path]:
    """Runs the pipeline against `app_yaml` and writes everything Stage
    3a/3b produce into `target_dir`. If `scaffold_src` is given, also
    scaffolds `main.c`/`janus_actions.c` there the first time they're
    needed (Stage 5/8) — never touched again once they exist. Returns
    every path actually written (empty on a no-op re-run)."""
    app = parse_app(app_yaml)
    for screen in app.screens:
        layout_screen(screen)
        if app.display is not None:
            check_fits_display(screen, app.display)

    written = write_project(app, target_dir)

    if scaffold_src is not None:
        scaffold_src = Path(scaffold_src)
        if scaffold_actions_c(app, scaffold_src / "janus_actions.c"):
            written.append(scaffold_src / "janus_actions.c")
        if scaffold_main_c(app, scaffold_src / "main.c"):
            written.append(scaffold_src / "main.c")

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
        help="also scaffold src/main.c and src/janus_actions.c under DIR if they don't exist yet",
    )
    args = parser.parse_args(argv)

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
