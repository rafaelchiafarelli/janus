#!/usr/bin/env bash
# Regenerates this example the same way any real consumer does: through
# scripts/janus.sh --target, not by importing janus.cli in-process.
# host_demo exists to prove Janus works the way a user actually
# experiences it, so it has to exercise that same path.
#
#   app.yaml -> build/generated/ + the Stage 5/8 src/ scaffolds
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JANUS_ROOT="$(cd "$HERE/../.." && pwd)"

"$JANUS_ROOT/scripts/janus.sh" "$HERE/app.yaml" \
    --target embedded_c "$HERE/build/generated" \
    --scaffold-src "$HERE/src"
