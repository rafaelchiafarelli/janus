#!/usr/bin/env bash
# Epic gate for desktop_scaffold: examples/desktop_demo generates, builds
# with no hand edits, runs headless, and quits cleanly. Needs cmake and
# SDL2 dev files. Run directly:
#
#   tests/test_desktop_demo.sh
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEMO="$REPO_ROOT/examples/desktop_demo"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail() { echo "FAIL: $1" >&2; exit 1; }

echo "== (a) generate.sh succeeds and never touches the committed scaffolds =="
before="$(cd "$DEMO/src" && sha256sum * | sort)"
"$DEMO/generate.sh" >/dev/null 2>&1 || fail "generate.sh failed"
after="$(cd "$DEMO/src" && sha256sum * | sort)"
[ "$before" = "$after" ] || fail "generate.sh modified the human-owned src/ files"
for f in main.c janus_actions.c desktop_input.c CMakeLists.txt; do
    [ -f "$DEMO/src/$f" ] || fail "src/$f missing"
done
echo "  ok"

echo "== (b) the demo configures and builds with no hand edits =="
cmake -S "$DEMO/src" -B "$WORK/build" -DJANUS_GENERATED_DIR="$DEMO/build/generated" >/dev/null \
    || fail "cmake configure failed"
cmake --build "$WORK/build" -j"$(nproc)" >/dev/null || fail "build failed"
[ -x "$WORK/build/janus_desktop_app" ] || fail "no janus_desktop_app binary"
echo "  ok"

echo "== (c) it starts headless and stays alive =="
export SDL_VIDEODRIVER=dummy
"$WORK/build/janus_desktop_app" &
pid=$!
sleep 1.5
kill -0 "$pid" 2>/dev/null || fail "app exited on its own (rc=$(wait "$pid" || true))"
echo "  ok"

echo "== (d) it exits 0 when asked to quit (SIGTERM -> SDL_QUIT) =="
kill -TERM "$pid"
rc=0; wait "$pid" || rc=$?
[ "$rc" -eq 0 ] || fail "expected a clean exit on quit, got rc=$rc"
echo "  ok"

echo "PASS"
