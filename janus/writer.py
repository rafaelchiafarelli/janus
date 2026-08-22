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


def copy_tree_if_changed(src_dir: str | Path, dest_dir: str | Path) -> list[Path]:
    """Recursively copies every file under `src_dir` into `dest_dir`,
    preserving the relative layout, via `write_if_changed` per file — so
    vendoring the fixed runtime library into a generated project never
    touches mtime on files whose content hasn't changed. Returns every
    destination path actually written (empty on a no-op re-run)."""
    src_dir = Path(src_dir)
    dest_dir = Path(dest_dir)
    written = []
    for src_path in sorted(src_dir.rglob("*")):
        if src_path.is_dir():
            continue
        dest_path = dest_dir / src_path.relative_to(src_dir)
        if write_if_changed(dest_path, src_path.read_text()):
            written.append(dest_path)
    return written
