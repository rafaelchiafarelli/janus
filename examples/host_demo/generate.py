"""Regenerates this example via the `janus-generate` CLI (janus/cli.py):
app.yaml -> `build/generated/` + the Stage 5/8 `src/` scaffolds.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))  # repo root, so `import janus` works

from janus.cli import main as cli_main

if __name__ == "__main__":
    sys.exit(cli_main([
        str(HERE / "app.yaml"),
        str(HERE / "build" / "generated"),
        "--scaffold-src", str(HERE / "src"),
    ]))
