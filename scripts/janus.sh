#!/usr/bin/env bash
# The one sanctioned way to run Janus as a consumer. `python -m
# janus.cli` (via janus/targets.py's TARGETS registry) always writes
# every implemented target it knows about, nested under a throwaway
# temp directory this script owns — this script's job is to install
# only the target(s) you actually asked for, flat, into the paths you gave
# it, exactly the shape every consumer already expects regardless of how
# many targets Janus internally knows about.
#
#   scripts/janus.sh <app.yaml> --target <name> <dest-dir> [--target <name> <dest-dir> ...] [--scaffold-src DIR]
#
# One run, any number of targets: each --target takes its own destination.
# At least one is required; a name may not repeat. If any requested target
# has no generator yet (e.g. `android`, until its own epic lands) the
# whole run fails loudly before anything is installed anywhere.
#
# --scaffold-src DIR: with one --target, main.c/janus_actions.c land flat
# in DIR; with several, each target's land in DIR/<target>/ (they differ
# per target). Existing files are never overwritten.
#
# Examples:
#   scripts/janus.sh examples/host_demo/app.yaml --target embedded_c examples/host_demo/build/generated --scaffold-src examples/host_demo/src
#   scripts/janus.sh app.yaml --target embedded_c out/embedded --target desktop out/desktop --scaffold-src "$(mktemp -d)"
#
# Calling `python -m janus.cli` directly is no longer a supported way
# for a consumer to run Janus — see the desktop_target initiative's
# multi_target_pipeline epic for why.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    sed -n '2,25p' "${BASH_SOURCE[0]}" | sed 's/^#\( \|$\)//' >&2
    exit 1
}

[ "$#" -eq 0 ] && usage

targets=()
dests=()
scaffold_src=""
positional=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        --target)
            [ "$#" -ge 3 ] || { echo "janus.sh: --target needs <name> <dest-dir>" >&2; exit 2; }
            for t in "${targets[@]+"${targets[@]}"}"; do
                [ "$t" != "$2" ] || { echo "janus.sh: target '$2' given twice" >&2; exit 2; }
            done
            targets+=("$2")
            dests+=("$3")
            shift 3
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

if [ "${#positional[@]}" -ne 1 ]; then
    echo "janus.sh: expected exactly one <app.yaml>, got ${#positional[@]} positional argument(s)" >&2
    usage
fi
app_yaml="${positional[0]}"

[ "${#targets[@]}" -ge 1 ] || { echo "janus.sh: at least one --target <name> <dest-dir> is required" >&2; exit 2; }

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

# Validate every requested target before touching any destination.
for target in "${targets[@]}"; do
    if [ ! -d "$tmp/gen/$target" ]; then
        echo "janus.sh: target '$target' has no generator yet" >&2
        exit 1
    fi
done

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

for i in "${!targets[@]}"; do
    target="${targets[$i]}"
    install_diffed "$tmp/gen/$target" "${dests[$i]}"

    if [ -n "$scaffold_src" ]; then
        target_scaffold="$tmp/scaffold/$target"
        if [ -d "$target_scaffold" ]; then
            if [ "${#targets[@]}" -gt 1 ]; then
                scaffold_dest="$scaffold_src/$target"
            else
                scaffold_dest="$scaffold_src"
            fi
            mkdir -p "$scaffold_dest"
            # Never clobber a file that already exists — human-owned once
            # scaffolded, matching write_if_missing's guarantee on the
            # Python side.
            find "$target_scaffold" -type f -print0 | while IFS= read -r -d '' f; do
                rel="${f#"$target_scaffold"/}"
                out="$scaffold_dest/$rel"
                if [ ! -f "$out" ]; then
                    mkdir -p "$(dirname "$out")"
                    cp "$f" "$out"
                    echo "scaffolded $out"
                fi
            done
        fi
    fi
done
