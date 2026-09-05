#!/usr/bin/env bash
# Runs Janus's generator (janus/cli.py) against an app.yaml and writes
# everything — generated widget/screen descriptors, bindings, actions
# header, the scaffolded main.c/janus_actions.c, AND the vendored
# runtime/embedded_c library — into one destination folder, instead of
# the generated/ vs src/ split examples/host_demo uses.
# Scaffolded files are only ever written the first time they're needed
# (janus.cli's own rule — see architecture.md); re-running this script is
# always safe, content-diffed like every other Janus output.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
    cat >&2 <<EOF
Usage: $(basename "$0") <app.yaml> <dest-dir>

  <app.yaml>   path to the project's app.yaml (the origin file)
  <dest-dir>   directory to write generated + scaffolded files into.
               Generated .c/.h land under <dest-dir>/src and
               <dest-dir>/include; main.c/janus_actions.c are scaffolded
               directly under <dest-dir>; the fixed runtime/embedded_c
               library is vendored into <dest-dir>/runtime, making
               <dest-dir> buildable on its own (CMakeLists.txt included).
EOF
    exit 1
}

[ "$#" -eq 2 ] || usage

app_yaml=$1
dest_dir=$2

[ -f "$app_yaml" ] || { echo "error: $app_yaml: no such file" >&2; exit 1; }

mkdir -p "$dest_dir"

args=("$app_yaml" "$dest_dir" --scaffold-src "$dest_dir")

if command -v janus-generate >/dev/null 2>&1; then
    janus-generate "${args[@]}"
else
    PYTHONPATH="$REPO_ROOT${PYTHONPATH:+:$PYTHONPATH}" python3 -m janus.cli "${args[@]}"
fi
