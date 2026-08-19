"""Human-file scaffolding for Stage 5 and Stage 8. See architecture.md.

Both `src/janus_actions.c` and `src/main.c` are human-owned: written
once, never regenerated. Each resolves an open question architecture.md
left unanswered ("does Janus scaffold this file ... or does the human
create it from scratch?") the same way: yes, Janus writes a starter the
first time it's needed.

Deliberately doesn't use writer.write_if_changed: that overwrites
whenever content differs, which is exactly the "regenerate a human
file" behavior these stages forbid. `_scaffold` only ever writes when
the target doesn't exist at all, and never touches it again after that.
"""
from __future__ import annotations

from pathlib import Path

from .emit_embedded_c import action_enum_name, collect_on_press_actions
from .ir import App
from .templates import load_template


def _scaffold(path: str | Path, content: str) -> bool:
    path = Path(path)
    if path.exists():
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    return True


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
    return _scaffold(path, render_actions_c(app))


def render_main_c() -> str:
    """Static — the documented boot sequence (driver init, render the
    active screen from the generated janus_app table) doesn't depend on
    any per-app data."""
    return load_template("main.c.tmpl")


def scaffold_main_c(path: str | Path) -> bool:
    """Writes a starter main.c only if `path` doesn't exist yet. Same
    once-only semantics as scaffold_actions_c."""
    return _scaffold(path, render_main_c())
