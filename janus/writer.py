"""Content-diff-before-write file writer.

Janus's own deliberate improvement over harpia's unconditional
`open(path, "w").write(...)` (see Janus.md / architecture.md): the
files this writes get compiled, so touching mtime on byte-identical
content would force a wasted recompile on every regeneration.
"""
from __future__ import annotations

from pathlib import Path


def write_if_changed(path: str | Path, content: str) -> bool:
    """Writes `content` to `path` only if it differs from what's already
    on disk (or the file doesn't exist yet). Returns True if a write
    happened, False if the file was left untouched."""
    path = Path(path)
    if path.exists() and path.read_text() == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    return True


def write_if_missing(path: str | Path, content: str) -> bool:
    """Writes `content` to `path` only if nothing is there yet. Used for
    human-owned scaffolds (Stage 5 `src/janus_actions.c`, Stage 8
    `src/main.c`): unlike `write_if_changed`, an existing file is left
    untouched even if its content differs — Janus never regenerates a
    human file once it exists. Returns True if it scaffolded a new file."""
    path = Path(path)
    if path.exists():
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    return True
