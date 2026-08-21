"""Human-file scaffolding for Stage 8. See architecture.md.

`src/main.c` is human-owned: written once, never regenerated. Same
once-only resolution as Stage 5's `src/janus_actions.c` — Janus writes
a starter the first time it's needed, then never touches it again.

Stage 6 added two more input modalities (encoder, buttons) alongside
touch — each needs a different event-loop shape (poll a point vs. poll a
focus move/activate), so there's one static `.tmpl` per modality rather
than one template with conditionals; `app.input_modality` (from
`app.yaml`'s `input:` block, Stage 1) picks which one gets scaffolded.
"""
from __future__ import annotations

from pathlib import Path

from ..ir import App, InputModality
from ..templates import load_template
from ..writer import write_if_missing

_TEMPLATE_BY_MODALITY: dict[InputModality, str] = {
    "touch": "main_touch.c.tmpl",
    "encoder": "main_encoder.c.tmpl",
    "buttons": "main_buttons.c.tmpl",
}


def render_main_c(modality: InputModality = "touch") -> str:
    """Static per modality — the documented boot sequence (driver init,
    render the active screen from the generated janus_app table) doesn't
    depend on any other per-app data."""
    return load_template(_TEMPLATE_BY_MODALITY[modality])


def scaffold_main_c(app: App, path: str | Path) -> bool:
    """Writes a starter main.c (for `app.input_modality`) only if `path`
    doesn't exist yet. Same once-only semantics as scaffold_actions_c."""
    return write_if_missing(path, render_main_c(app.input_modality))
