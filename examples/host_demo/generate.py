"""One-off glue for this example: app.yaml/*.screen.yaml -> App IR ->
layout -> write_project()'s `build/generated/` + the Stage 5/8 human-file
scaffolds. Not a Janus CLI (none exists yet) — just enough to prove the
pipeline runs end to end for this trivial one-screen example.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))  # repo root, so `import janus` works

from janus.stage1_parse.dsl_yaml import parse_app
from janus.stage2_layout.layout import layout_screen
from janus.stage5_actions.scaffold_actions import scaffold_actions_c
from janus.stage8_scaffold.scaffold_main import scaffold_main_c
from janus.generate import write_project


def main() -> None:
    app = parse_app(HERE / "app.yaml")
    for screen in app.screens:
        layout_screen(screen)

    written = write_project(app, HERE / "build" / "generated")
    for path in written:
        print(f"wrote {path.relative_to(HERE)}")

    if scaffold_actions_c(app, HERE / "src" / "janus_actions.c"):
        print("scaffolded src/janus_actions.c")
    if scaffold_main_c(HERE / "src" / "main.c"):
        print("scaffolded src/main.c")


if __name__ == "__main__":
    main()
