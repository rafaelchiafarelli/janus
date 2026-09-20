#!/usr/bin/env bash
# The one sanctioned way to run Janus as a consumer. `python -m
# janus.cli` (via janus/targets.py's TARGETS registry) always writes
# every implemented target it knows about, nested under a throwaway
# temp directory this script owns — this script's job is to install
# only the target you actually asked for, flat, into the paths you gave
# it, exactly the shape every consumer already expects regardless of how
# many targets Janus internally knows about.
#
#   scripts/janus.sh <app.yaml> <dest-dir> --target <name> [--scaffold-src DIR]
#
# --target is required. A name with no generator yet (e.g. `desktop`,
# `android`, until their own epics land) fails loudly, no partial copy
# left in <dest-dir>.
#
# Examples:
#   scripts/janus.sh examples/host_demo/app.yaml examples/host_demo/build/generated --target embedded_c --scaffold-src examples/host_demo/src
#   scripts/janus.sh /path/to/board/app.yaml /path/to/board/gui --target embedded_c --scaffold-src "$(mktemp -d)"   # keep scaffold main.c out of the tree
#
# Calling `python -m janus.cli` directly is no longer a supported way
# for a consumer to run Janus — see the desktop_target initiative's
# multi_target_pipeline epic for why.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^#\( \|$\)//' >&2
    exit 1
}

[ "$#" -eq 0 ] && usage

target=""
scaffold_src=""
positional=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        --target)
            [ "$#" -ge 2 ] || { echo "janus.sh: --target needs a value" >&2; exit 2; }
            target="$2"
            shift 2
            ;;
        --scaffold-src)
            [ "$#" -ge 2 ] || { echo "janus.sh: --scaffold-src needs a value" >&2; exit 2; }
            scaffold_src="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        --*)
            echo "janus.sh: unknown flag $1" >&2
            exit 2
            ;;
        *)
            positional+=("$1")
            shift
            ;;
    esac
done

if [ "${#positional[@]}" -ne 2 ]; then
    echo "janus.sh: expected <app.yaml> <dest-dir>, got ${#positional[@]} positional argument(s)" >&2
    usage
fi
app_yaml="${positional[0]}"
dest_dir="${positional[1]}"

[ -n "$target" ] || { echo "janus.sh: --target <name> is required" >&2; exit 2; }

# Prefer the repo venv; fall back to whatever python3 is on PATH.
if [ -x "$REPO_ROOT/.venv/bin/python" ]; then
    py="$REPO_ROOT/.venv/bin/python"
else
    py="$(command -v python3 || command -v python || true)"
fi
[ -n "$py" ] || { echo "janus.sh: no python interpreter found" >&2; exit 1; }

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

gen_args=("$app_yaml" "$tmp/gen")
[ -n "$scaffold_src" ] && gen_args+=(--scaffold-src "$tmp/scaffold")
env PYTHONPATH="$REPO_ROOT${PYTHONPATH:+:$PYTHONPATH}" "$py" -m janus.cli "${gen_args[@]}"

target_gen="$tmp/gen/$target"
if [ ! -d "$target_gen" ]; then
    echo "janus.sh: target '$target' has no generator yet" >&2
    exit 1
fi

# Content-diffed install: only copy a file if it's new or its content
# differs, so an unchanged file's mtime is never touched — same
# discipline write_if_changed already gives every other Janus output.
install_diffed() {
    local src="$1" dest="$2"
    mkdir -p "$dest"
    find "$src" -type f -print0 | while IFS= read -r -d '' f; do
        rel="${f#"$src"/}"
        out="$dest/$rel"
        if [ ! -f "$out" ] || ! cmp -s "$f" "$out"; then
            mkdir -p "$(dirname "$out")"
            cp "$f" "$out"
            echo "installed $out"
        fi
    done
}

install_diffed "$target_gen" "$dest_dir"

if [ -n "$scaffold_src" ]; then
    target_scaffold="$tmp/scaffold/$target"
    if [ -d "$target_scaffold" ]; then
        mkdir -p "$scaffold_src"
        # Never clobber a file that already exists — human-owned once
        # scaffolded, matching write_if_missing's guarantee on the
        # Python side.
        find "$target_scaffold" -type f -print0 | while IFS= read -r -d '' f; do
            rel="${f#"$target_scaffold"/}"
            out="$scaffold_src/$rel"
            if [ ! -f "$out" ]; then
                mkdir -p "$(dirname "$out")"
                cp "$f" "$out"
                echo "scaffolded $out"
            fi
        done
    fi
fi
