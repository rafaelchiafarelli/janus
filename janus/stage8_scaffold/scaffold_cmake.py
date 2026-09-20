"""Desktop app CMakeLists.txt scaffolding for Stage 8. See architecture.md.

Written once next to the desktop `main.c` / `desktop_input.c`, then
human-owned like them. Janus can't know where the generated tree lives
relative to the scaffold folder, so the template takes a
`JANUS_GENERATED_DIR` cache variable and globs `src/*.gen.c` from it —
a static template, nothing per-app baked in.
"""
from __future__ import annotations

from pathlib import Path

from ..ir import App
from ..templates import load_template
from ..writer import write_if_missing


def render_desktop_cmake() -> str:
    return load_template("desktop_app_CMakeLists.txt.tmpl")


def scaffold_desktop_cmake(app: App, path: str | Path) -> bool:
    """Writes the app CMakeLists.txt only if `path` doesn't exist yet."""
    return write_if_missing(path, render_desktop_cmake())
