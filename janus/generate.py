"""Ties every Stage 3a/3b emitter + the .tmpl wrappers to disk: produces
a full `build/generated/`-style tree from an `App` IR for the first
time. Uses write_if_changed throughout, so a regeneration that changes
nothing touches no mtimes and forces no recompile.
"""
from __future__ import annotations

from pathlib import Path

from .ir import App
from .stage3a_harpia.emit_harpia import emit_harpia
from .stage3b_embedded_c.emit_embedded_c import screen_index_map, screen_var
from .stage3b_embedded_c.emit_files import (
    render_actions_header,
    render_app_source,
    render_bindings_header,
    render_bindings_source,
    render_display_config_header,
    render_screen_header,
    render_screen_source,
)
from .writer import write_if_changed


def write_project(app: App, out_dir: str | Path) -> list[Path]:
    """Writes, under `out_dir`: `{screen}_screen.gen.{h,c}` per screen,
    `janus_actions.gen.h`, `janus_app.gen.c`, `janus_bindings.gen.{h,c}`,
    `janus_generated.harpia`, and — only if `app.display` is set —
    `janus_display_config.gen.h`. Returns the paths actually written —
    content-diff-before-write means a no-op regeneration returns an
    empty list."""
    out_dir = Path(out_dir)
    index = screen_index_map(app)
    written: list[Path] = []

    def _write(path: Path, content: str) -> None:
        if write_if_changed(path, content):
            written.append(path)

    for screen in app.screens:
        sv = screen_var(screen.name)
        _write(out_dir / f"{sv}_screen.gen.h", render_screen_header(screen))
        _write(out_dir / f"{sv}_screen.gen.c", render_screen_source(screen, index))

    _write(out_dir / "janus_actions.gen.h", render_actions_header(app))
    _write(out_dir / "janus_app.gen.c", render_app_source(app))
    _write(out_dir / "janus_bindings.gen.h", render_bindings_header(app))
    _write(out_dir / "janus_bindings.gen.c", render_bindings_source(app))
    _write(out_dir / "janus_generated.harpia", emit_harpia(app))

    if app.display is not None:
        _write(out_dir / "janus_display_config.gen.h", render_display_config_header(app.display))

    return written
