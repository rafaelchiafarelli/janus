"""Wraps Stage 3b's raw declarations (emit_embedded_c.py) into complete,
standalone .c/.h files — header guards, #includes. This is exactly the
piece the Stage 3b slice commit deliberately deferred: "no .tmpl
wrapper/#includes ... those are separate later increments."

Mechanism matches Janus.md's stated approach for this project:
harpia-style `.tmpl` + `str.format()`, no templating engine. Each
render_* function here loads one `.tmpl` file and fills in the single
`{body}`/`{guard}` placeholders it needs — the brace-heavy C content
itself comes from emit_embedded_c.py and is substituted as opaque text,
so it never needs escaping.
"""
from __future__ import annotations

from .emit_embedded_c import emit_actions_header, emit_app_table, emit_screen, screen_var
from .ir import App, Screen
from .templates import load_template


def _guard(*parts: str) -> str:
    return "JANUS_GEN_" + "_".join(p.upper() for p in parts) + "_H"


def render_screen_header(screen: Screen) -> str:
    sv = screen_var(screen.name)
    return load_template("screen.h.tmpl").format(guard=_guard(sv, "screen"), screen_var=sv)


def render_screen_source(
    screen: Screen, screen_index_by_name: dict[str, int] | None = None
) -> str:
    sv = screen_var(screen.name)
    body = emit_screen(screen, screen_index_by_name)
    return load_template("screen.c.tmpl").format(screen_var=sv, body=body)


def render_actions_header(app: App) -> str:
    body = emit_actions_header(app)
    return load_template("actions.h.tmpl").format(guard=_guard("actions"), body=body)


def render_app_source(app: App) -> str:
    body = emit_app_table(app)
    return load_template("app.c.tmpl").format(body=body)
