"""Desktop input scaffolding for Stage 8. See architecture.md.

The desktop `main.c` (`main_desktop_*.c.tmpl`) calls the same
`janus_{touch,encoder,buttons}_poll` contract every target does; on
embedded hardware the vendor supplies it, on desktop Janus scaffolds a
default SDL implementation once into `src/desktop_input.c` — mouse for
touch, keyboard for encoder/buttons. Same once-only semantics as
`main.c`/`janus_actions.c`: after the first write the file is human-owned
and never touched again, so the key mapping stays the human's to change.
"""
from __future__ import annotations

from pathlib import Path

from ..ir import App, InputModality
from ..templates import load_template
from ..writer import write_if_missing

_TEMPLATE_BY_MODALITY: dict[InputModality, str] = {
    "touch": "desktop_input_touch.c.tmpl",
    "encoder": "desktop_input_encoder.c.tmpl",
    "buttons": "desktop_input_buttons.c.tmpl",
}


def render_desktop_input_c(modality: InputModality = "touch") -> str:
    return load_template(_TEMPLATE_BY_MODALITY[modality])


def scaffold_desktop_input_c(app: App, path: str | Path) -> bool:
    """Writes the default desktop input file for `app.input_modality`
    only if `path` doesn't exist yet."""
    return write_if_missing(path, render_desktop_input_c(app.input_modality))


def scaffold_mirror_link_c(app: App, path: str | Path) -> bool:
    """Mirror mode's replacement for `desktop_input.c`: the once-only
    transport hook (`mirror_link_poll`). Same once-only semantics."""
    return write_if_missing(path, load_template("mirror_link.c.tmpl"))
