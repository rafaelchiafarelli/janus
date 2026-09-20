"""Human-file scaffolding for Stage 8. See architecture.md.

`src/main.c` is human-owned: written once, never regenerated. Same
once-only resolution as Stage 5's `src/janus_actions.c` — Janus writes
a starter the first time it's needed, then never touches it again.

Stage 6 added two more input modalities (encoder, buttons) alongside
touch — each needs a different event-loop shape (poll a point vs. poll a
focus move/activate), so there's one static `.tmpl` per modality rather
than one template with conditionals; `app.input_modality` (from
`app.yaml`'s `input:` block, Stage 1) picks which one gets scaffolded.

Non-blocking rendering (`app.yaml`'s `display.render_mode`) cuts across
modality the same way: it changes how the active screen gets (re)rendered
(`janus_render_screen` once vs. `janus_render_screen_async_start` +
polling `janus_render_poll` alongside the input poll) but not how input
itself is polled — so it's a second axis of the same one-static-template-
per-combination approach, not a conditional inside each modality template.
"""
from __future__ import annotations

from pathlib import Path

from ..ir import App, InputModality, RenderMode
from ..templates import load_template
from ..writer import write_if_missing

_TEMPLATE_BY_MODALITY_AND_RENDER_MODE: dict[tuple[InputModality, RenderMode], str] = {
    ("touch", "blocking"): "main_touch.c.tmpl",
    ("encoder", "blocking"): "main_encoder.c.tmpl",
    ("buttons", "blocking"): "main_buttons.c.tmpl",
    ("touch", "non_blocking"): "main_touch_async.c.tmpl",
    ("encoder", "non_blocking"): "main_encoder_async.c.tmpl",
    ("buttons", "non_blocking"): "main_buttons_async.c.tmpl",
}

# The desktop target has no async variants: its draw_area_async is
# always instant (desktop_sdl2_runtime epic decision 5), so a
# `non_blocking` app.yaml still runs correctly on the blocking loop.
_DESKTOP_TEMPLATE_BY_MODALITY: dict[InputModality, str] = {
    "touch": "main_desktop_touch.c.tmpl",
    "encoder": "main_desktop_encoder.c.tmpl",
    "buttons": "main_desktop_buttons.c.tmpl",
}


def render_main_c(
    modality: InputModality = "touch", render_mode: RenderMode = "blocking", target: str = "embedded_c",
) -> str:
    """Static per (target, modality, render_mode) — the documented boot
    sequence (driver init, render the active screen from the generated
    janus_app table) doesn't depend on any other per-app data. `desktop`
    ignores `render_mode` (see _DESKTOP_TEMPLATE_BY_MODALITY)."""
    if target == "desktop":
        return load_template(_DESKTOP_TEMPLATE_BY_MODALITY[modality])
    return load_template(_TEMPLATE_BY_MODALITY_AND_RENDER_MODE[(modality, render_mode)])


def scaffold_main_c(app: App, path: str | Path, target: str = "embedded_c") -> bool:
    """Writes a starter main.c (for `target`, `app.input_modality` and
    `app.display.render_mode`, if `app.display` is set — `blocking`
    otherwise, matching today's behavior) only if `path` doesn't exist
    yet. Same once-only semantics as scaffold_actions_c."""
    render_mode = app.display.render_mode if app.display is not None else "blocking"
    return write_if_missing(path, render_main_c(app.input_modality, render_mode, target))
