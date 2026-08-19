"""Stage 5 — action dispatch scaffolding. See architecture.md.

`src/janus_actions.c` is human-owned: written once, never regenerated.
Resolves architecture.md's own open question ("does Janus scaffold this
file with TODO cases the first time it's needed, or does the human
create it from scratch?") — yes, Janus scaffolds a starter with one
TODO case per known action.

Deliberately doesn't use writer.write_if_changed: that overwrites
whenever content differs, which is exactly the "regenerate a human
file" behavior Stage 5 forbids. This only ever writes when the file
doesn't exist at all, and never touches it again after that.
"""
from __future__ import annotations

from pathlib import Path

from .emit_embedded_c import action_enum_name, collect_on_press_actions
from .ir import App
from .templates import load_template


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
    path = Path(path)
    if path.exists():
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(render_actions_c(app))
    return True
