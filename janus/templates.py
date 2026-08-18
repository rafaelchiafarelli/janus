"""Loads the harpia-style `.tmpl` files under `janus/templates/`.

Matches harpia's own mechanism (`Util/util.py`'s `loadTemplate`): plain
text files with `str.format()`-style `{placeholder}` markers, no
templating engine. See Janus.md's "Embedded-C code generation
architecture" section.
"""
from __future__ import annotations

from pathlib import Path

_TEMPLATE_DIR = Path(__file__).parent / "templates"


def load_template(name: str) -> str:
    return (_TEMPLATE_DIR / name).read_text()
