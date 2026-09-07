#!/usr/bin/env bash
# One-shot dev environment setup for janus. Idempotent — safe to re-run.
#
#   ./environment.sh          # set up the Python venv; report missing system tools
#   ./environment.sh --apt    # also `sudo apt-get install` the missing system tools
#
# What it provisions:
#   * .venv/            Python venv with janus + its deps (PyYAML, Pillow) —
#                       Pillow is what makes tests/test_image_asset.py and
#                       tests/test_emit_embedded_c.py's image cases run instead
#                       of erroring/skipping.
#   * system packages   cmake + a C toolchain (runtime/embedded_c ctest suite),
#                       gcc-avr + avr-libc (scripts/avr_gate.sh).
#
# After running, use the venv for the Python suite:
#   .venv/bin/python -m unittest discover -s tests
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

WITH_APT=0
[ "${1:-}" = "--apt" ] && WITH_APT=1

# ---------------------------------------------------------------- python --
if [ ! -d .venv ]; then
    echo "== creating .venv =="
    python3 -m venv .venv
fi

echo "== installing janus + deps into .venv (editable) =="
.venv/bin/python -m pip install --upgrade pip >/dev/null
.venv/bin/python -m pip install -e .          # PyYAML + Pillow>=10, from pyproject.toml

echo "== python suite =="
.venv/bin/python -m unittest discover -s tests -q && echo "  ok" || {
    echo "  python suite is RED — see output above" >&2
    exit 1
}

# --------------------------------------------------------------- system --
# name -> apt package
declare -A NEED=(
    [cmake]=cmake
    [ctest]=cmake
    [gcc]=build-essential
    [make]=build-essential
    [avr-gcc]=gcc-avr
    [avr-objcopy]=binutils-avr
)
missing_pkgs=()
for tool in "${!NEED[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "  missing: $tool  (apt: ${NEED[$tool]})"
        missing_pkgs+=("${NEED[$tool]}")
    fi
done
# avr-libc ships headers, not a binary on PATH — check a known header instead
if [ ! -e /usr/lib/avr/include/avr/io.h ] && ! ls /usr/avr/include/avr/io.h >/dev/null 2>&1; then
    echo "  missing: avr-libc  (apt: avr-libc)"
    missing_pkgs+=(avr-libc)
fi

if [ "${#missing_pkgs[@]}" -eq 0 ]; then
    echo "== system tools: all present =="
else
    # dedupe
    mapfile -t missing_pkgs < <(printf '%s\n' "${missing_pkgs[@]}" | sort -u)
    if [ "$WITH_APT" -eq 1 ]; then
        echo "== installing: ${missing_pkgs[*]} (sudo) =="
        sudo apt-get update
        sudo apt-get install -y "${missing_pkgs[@]}"
    else
        echo
        echo "System packages are missing. Install them with:"
        echo "    sudo apt-get install -y ${missing_pkgs[*]}"
        echo "or re-run:  ./environment.sh --apt"
    fi
fi

echo
echo "done."
