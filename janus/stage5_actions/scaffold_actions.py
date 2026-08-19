"""Human-file scaffolding for Stage 5. See architecture.md.

`src/janus_actions.c` is human-owned: written once, never regenerated.
Resolves an open question architecture.md left unanswered ("does Janus
scaffold this file ... or does the human create it from scratch?"):
yes, Janus writes a starter the first time it's needed.

Uses writer.write_if_missing, not write_if_changed: that overwrites
whenever content differs, which is exactly the "regenerate a human
file" behavior this stage forbids.
"""
from __future__ import annotations

from pathlib import Path

from ..ir import App
from ..templates import load_template
from ..writer import write_if_missing
from ..stage3b_embedded_c.emit_embedded_c import action_enum_name, collect_on_press_actions


def render_actions_c(app: App) -> str:
    cases = "\n".join(
        f"        case {action_enum_name(a)}: /* TODO */ break;"
        for a in collect_on_press_actions(app)
    )
    return load_template("actions.c.tmpl").format(cases=cases)


def scaffold_actions_c(app: App, path: str | Path) -> bool:
    """Writes a starter janus_actions.c only if `path` doesn't exist
    yet. Returns True if it scaffolded a new file, False if one was
    already there (left untouched — human-owned once it exists)."""
    return write_if_missing(path, render_actions_c(app))
