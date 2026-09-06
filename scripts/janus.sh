#!/usr/bin/env bash
# Thin wrapper around Janus's CLI (janus/cli.py): forwards every argument
# straight through to `python -m janus.cli`, using the repo's own venv and
# making `janus` importable regardless of the current directory.
#
#   scripts/janus.sh <app.yaml> <target-dir> [--scaffold-src DIR]
#
# Examples:
#   scripts/janus.sh examples/host_demo/app.yaml examples/host_demo/build/generated --scaffold-src examples/host_demo/src
#   scripts/janus.sh /path/to/board/app.yaml /path/to/board/gui --scaffold-src "$(mktemp -d)"   # keep scaffold main.c out of the tree
#
# See `python -m janus.cli --help` for the full argument list.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "$#" -eq 0 ]; then
    sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^#\( \|$\)//' >&2
    exit 1
fi

# Prefer the repo venv; fall back to whatever python3 is on PATH.
if [ -x "$REPO_ROOT/.venv/bin/python" ]; then
    PY="$REPO_ROOT/.venv/bin/python"
else
    PY="$(command -v python3 || command -v python)"
fi
[ -n "${PY:-}" ] || { echo "janus.sh: no python interpreter found" >&2; exit 1; }

exec env PYTHONPATH="$REPO_ROOT${PYTHONPATH:+:$PYTHONPATH}" "$PY" -m janus.cli "$@"
