#!/usr/bin/env bash
# Shell-level coverage for scripts/janus.sh --target (multi_target_pipeline
# epic, task 2). Not part of `python -m unittest discover` — this exercises
# the shell script itself, not Python. Run directly:
#
#   tests/test_janus_sh.sh
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JANUS_SH="$REPO_ROOT/scripts/janus.sh"
FIXTURE="$REPO_ROOT/tests/fixtures/app.yaml"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

echo "== (a) --target embedded_c reproduces python -m janus.cli's own output, flat =="
DIRECT="$WORK/direct"
env PYTHONPATH="$REPO_ROOT" python3 -m janus.cli "$FIXTURE" "$DIRECT/gen" --scaffold-src "$DIRECT/scaffold" \
    >/dev/null
VIA_SCRIPT="$WORK/via_script"
"$JANUS_SH" "$FIXTURE" --target embedded_c "$VIA_SCRIPT/gen" --scaffold-src "$VIA_SCRIPT/scaffold" \
    >/dev/null
diff -r "$DIRECT/gen/embedded_c" "$VIA_SCRIPT/gen" \
    || fail "installed output differs from python -m janus.cli's own embedded_c subtree"
diff -r "$DIRECT/scaffold/embedded_c" "$VIA_SCRIPT/scaffold" \
    || fail "installed scaffold differs from python -m janus.cli's own embedded_c subtree"
echo "  ok"

echo "== (b) second run with no spec changes copies nothing new (mtimes untouched) =="
before="$(find "$VIA_SCRIPT/gen" -type f -newer "$JANUS_SH" | sort)"
sleep 1.1  # coarse mtime resolution safety margin
out="$("$JANUS_SH" "$FIXTURE" --target embedded_c "$VIA_SCRIPT/gen" --scaffold-src "$VIA_SCRIPT/scaffold")"
if echo "$out" | grep -qE '^(installed|scaffolded) '; then
    fail "second run with no changes reported writes: $out"
fi
echo "  ok"

echo "== (c) an unimplemented target fails loudly, no partial copy left behind =="
DEST="$WORK/unimplemented_dest"
if "$JANUS_SH" "$FIXTURE" --target android "$DEST" >/dev/null 2>"$WORK/stderr"; then
    fail "expected --target android to fail"
fi
grep -q "target 'android' has no generator yet" "$WORK/stderr" \
    || fail "missing/wrong error message: $(cat "$WORK/stderr")"
[ ! -e "$DEST" ] || fail "$DEST was created despite the target having no generator"
echo "  ok"

echo "== (d) --scaffold-src never clobbers a pre-existing scaffold file =="
CLOBBER_TEST="$WORK/clobber_test"
mkdir -p "$CLOBBER_TEST/src"
echo "/* hand-edited, must survive */" > "$CLOBBER_TEST/src/main.c"
"$JANUS_SH" "$FIXTURE" --target embedded_c "$CLOBBER_TEST/gen" --scaffold-src "$CLOBBER_TEST/src" \
    >/dev/null
[ "$(cat "$CLOBBER_TEST/src/main.c")" = "/* hand-edited, must survive */" ] \
    || fail "main.c was overwritten"
[ -f "$CLOBBER_TEST/src/janus_actions.c" ] \
    || fail "janus_actions.c should have been freshly scaffolded (no pre-existing file)"
echo "  ok"

echo "== (e) missing --target is a usage error =="
if "$JANUS_SH" "$FIXTURE" 2>/dev/null; then
    fail "expected a missing --target to fail"
fi
echo "  ok"

echo "== (f) --target desktop installs a real, buildable runtime =="
DESKTOP_DEST="$WORK/desktop_dest"
"$JANUS_SH" "$FIXTURE" --target desktop "$DESKTOP_DEST" --scaffold-src "$WORK/desktop_scaffold" >/dev/null
[ -f "$DESKTOP_DEST/runtime/CMakeLists.txt" ] || fail "no runtime/CMakeLists.txt installed for desktop"
[ -f "$DESKTOP_DEST/runtime/driver/janus_desktop_driver.c" ] \
    || fail "the SDL2 driver itself wasn't installed"
cmake -S "$DESKTOP_DEST/runtime" -B "$DESKTOP_DEST/build" >/dev/null \
    || fail "desktop runtime failed to cmake-configure"
cmake --build "$DESKTOP_DEST/build" -j"$(nproc)" >/dev/null \
    || fail "desktop runtime failed to build"
echo "  ok"

echo "== (g) two targets in one run land in two destinations =="
MULTI="$WORK/multi"
"$JANUS_SH" "$FIXTURE" --target embedded_c "$MULTI/emb" --target desktop "$MULTI/desk" \
    --scaffold-src "$MULTI/src" >/dev/null
[ -f "$MULTI/emb/janus_generated.harpia" ] || fail "embedded_c not installed"
[ -f "$MULTI/desk/runtime/driver/janus_desktop_driver.c" ] || fail "desktop not installed"
[ ! -e "$MULTI/emb/runtime/driver" ] || fail "desktop files leaked into the embedded_c destination"
echo "  ok"

echo "== (i) multi-target --scaffold-src uses DIR/<target>/ =="
[ -f "$MULTI/src/embedded_c/main.c" ] || fail "missing $MULTI/src/embedded_c/main.c"
[ -f "$MULTI/src/desktop/main.c" ] || fail "missing $MULTI/src/desktop/main.c"
[ ! -e "$MULTI/src/main.c" ] || fail "flat main.c written despite multiple targets"
echo "  ok"

echo "== (h) an unimplemented target among valid ones installs nothing anywhere =="
PARTIAL="$WORK/partial"
if "$JANUS_SH" "$FIXTURE" --target embedded_c "$PARTIAL/emb" --target android "$PARTIAL/and" \
    >/dev/null 2>"$WORK/stderr2"; then
    fail "expected android among valid targets to fail"
fi
grep -q "target 'android' has no generator yet" "$WORK/stderr2" || fail "wrong error: $(cat "$WORK/stderr2")"
[ ! -e "$PARTIAL" ] || fail "$PARTIAL was created despite the abort"
echo "  ok"

echo "== (j) a duplicate target is a usage error =="
if "$JANUS_SH" "$FIXTURE" --target desktop "$WORK/d1" --target desktop "$WORK/d2" 2>/dev/null; then
    fail "expected a duplicate --target to fail"
fi
[ ! -e "$WORK/d1" ] || fail "$WORK/d1 was created despite the usage error"
echo "  ok"

echo "PASS"
