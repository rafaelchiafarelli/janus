#!/usr/bin/env bash
# Regenerates the desktop demo the way any real consumer does: through
# scripts/janus.sh. It renders host_demo's own app.yaml (and assets) — one
# spec, two targets — so there is no second copy of the spec to drift.
#
#   ../host_demo/app.yaml -> build/generated/ (regenerated every run)
#                         -> src/ (main.c, desktop_input.c, janus_actions.c,
#                                  CMakeLists.txt — scaffolded once, yours after)
#
# Then build and run (see README.md).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JANUS_ROOT="$(cd "$HERE/../.." && pwd)"

"$JANUS_ROOT/scripts/janus.sh" "$JANUS_ROOT/examples/host_demo/app.yaml" \
    --target desktop "$HERE/build/generated" \
    --scaffold-src "$HERE/src"
