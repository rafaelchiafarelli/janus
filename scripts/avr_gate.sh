#!/usr/bin/env bash
# channel_icons epic acceptance gate (tasks 3 + 4): prove the
# row-height-icon examples/host_demo actually links for a real
# ATmega2560 in its default `render_mode: blocking`.
#
#   1. regenerate examples/host_demo (app.yaml -> build/generated/)
#   2. avr-gcc -mmcu=atmega2560 -Os compile every generated .gen.c +
#      the fixed runtime src/*.c, with JANUS_RENDER_NONBLOCKING *absent*
#   3. link a minimal harness (stub driver contract + render-once main)
#      into an .elf — the link itself fails if .bss doesn't fit the
#      8 KiB SRAM region ("section `.bss' is not within region `data'")
#   4. assert avr-nm shows NONE of the async render path was linked
#      (g_async_ops, janus_render_poll, async_enqueue_*) — task 3
#   5. assert every near-read flash symbol (fonts, descriptors, strings,
#      screen tables — everything the runtime reads with near pgm_read_*)
#      links below 0x10000; only the baked-image `_px` arrays may sit
#      above it (read far, per task 2) — task 4
#   6. print avr-size so the SRAM / flash margin is visible
#
# Step 5 links with NO linker script, so it exercises avr-ld's orphan
# placement of `.janus_img` (the weaker guarantee); janus_img.ld +
# CMakeLists.txt pin it explicitly for a CMake consumer. Not wired into
# CI — run it by hand alongside the host suites when touching the
# embedded_rendering initiative.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST_DEMO="$REPO_ROOT/examples/host_demo"
RT="$REPO_ROOT/runtime/embedded_c"
GEN="$HOST_DEMO/build/generated"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PY="${PYTHON:-python3}"
command -v avr-gcc >/dev/null || { echo "avr-gcc not found" >&2; exit 1; }

echo "== regenerating examples/host_demo =="
"$PY" "$HOST_DEMO/generate.py"

echo "== compiling for -mmcu=atmega2560 (render_mode: blocking) =="
CFLAGS=(-mmcu=atmega2560 -Os -Wall -Wextra -DNDEBUG
        -ffunction-sections -fdata-sections
        -I"$RT/include" -I"$GEN/include")

cat > "$WORK/gate_main.c" <<'EOF'
/* Minimal ATmega2560 link harness — NOT real firmware. Stands in for the
 * vendor board's driver contract + entry point so the linker has to
 * place every generated descriptor / font / image array and the
 * runtime's .bss for a `render_mode: blocking` build. */
#include "janus_runtime.h"
extern janus_app_t janus_app;
void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *p) {
    (void)x; (void)y; (void)w; (void)h; (void)p;
}
bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *p) {
    (void)x; (void)y; (void)w; (void)h; (void)p; return true;
}
bool display_busy(void) { return false; }
int main(void) {
    janus_render_screen(janus_app_get_screen(&janus_app, janus_app.active_screen));
    for (;;) { }
}
EOF

OBJS=()
for src in "$GEN"/src/*.gen.c \
          "$RT"/src/janus_runtime.c "$RT"/src/janus_font.c \
          "$RT"/src/janus_input_touch.c "$RT"/src/janus_input_focus.c \
          "$WORK/gate_main.c"; do
    obj="$WORK/$(basename "${src%.c}").o"
    avr-gcc "${CFLAGS[@]}" -c "$src" -o "$obj"
    OBJS+=("$obj")
done

echo "== linking =="
avr-gcc -mmcu=atmega2560 -Os -Wl,--gc-sections \
        "${OBJS[@]}" -o "$WORK/host_demo.elf"

echo "== async render path must be absent (task 3) =="
if avr-nm "$WORK/host_demo.elf" \
     | grep -E ' (g_async_ops|g_async_op_count|g_async_cursor|g_async_enqueue|janus_render_poll|janus_render_screen_async_start|janus_switch_screen_async_start|async_enqueue_[a-z]+)$'; then
    echo "FAIL: non-blocking render path linked into a blocking build" >&2
    exit 1
fi
echo "  ok — none linked"

echo "== near-read flash data must stay < 0x10000 (task 4) =="
# text/rodata/data symbols (not RAM at 0x80xxxx, not undefined) whose VMA
# is >= 64 KiB and whose name is not a baked-image `_px` array.
viol="$(avr-nm "$WORK/host_demo.elf" | awk '
    NF == 3 && $2 ~ /^[tTrRdD]$/ {
        addr = strtonum("0x" $1)
        if (addr >= 0x10000 && addr < 0x800000 && $3 !~ /_px$/) print $1, $2, $3
    }')"
if [ -n "$viol" ]; then
    echo "FAIL: near-read symbol(s) linked past AVR's 64 KiB near window:" >&2
    echo "$viol" | sed 's/^/  /' >&2
    exit 1
fi
echo "  ok — all near-read data below 0x10000"

echo "== avr-size =="
avr-size -C --mcu=atmega2560 "$WORK/host_demo.elf"

echo "PASS"
