"""Human-file scaffolding for Stage 8. See architecture.md.

`src/main.c` is human-owned: written once, never regenerated. Same
once-only resolution as Stage 5's `src/janus_actions.c` — Janus writes
a starter the first time it's needed, then never touches it again.
"""
from __future__ import annotations

from pathlib import Path

from ..templates import load_template
from ..writer import write_if_missing


def render_main_c() -> str:
    """Static — the documented boot sequence (driver init, render the
    active screen from the generated janus_app table) doesn't depend on
    any per-app data."""
    return load_template("main.c.tmpl")


def scaffold_main_c(path: str | Path) -> bool:
    """Writes a starter main.c only if `path` doesn't exist yet. Same
    once-only semantics as scaffold_actions_c."""
    return write_if_missing(path, render_main_c())
