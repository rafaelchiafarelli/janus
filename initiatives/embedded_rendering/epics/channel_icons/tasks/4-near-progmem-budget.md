# Task 4: near-progmem-budget

Status: **spike done (2026-09-06) — approach chosen: "link image arrays
last". Awaiting Rafael's OK on the finalized contract below before a
branch is cut.**

## Problem

Task 2 made *image pixel* reads far-safe, but nothing else. ~80 KiB of
baked icon arrays in `.progmem` pushes the rest of the generated /
runtime flash data past the 64 KiB that **near** reads
(`pgm_read_byte`, `pgm_read_ptr`, `memcpy_P`) can address. Confirmed with
`avr-gcc -mmcu=atmega2560` on `examples/host_demo`: `janus_font_glyph_medium`
/ `janus_font_glyph_large` link at VMA `0x1b0xx` (> `0x10000`), and the
runtime reads them one byte at a time via `JANUS_PGM_READ_U8` →
garbage glyphs on the real board.

Also at risk once total near-`.progmem` crosses 64 KiB (not yet observed,
but the same mechanism): widget-descriptor arrays (`janus_widget_load`'s
`memcpy_P`), flash strings `id` / `static_text`, the
`janus_app_screens[]` pointer table (`pgm_read_ptr`), nav titles.

## Contract

After this task, `avr-gcc -mmcu=atmega2560` links `examples/host_demo`
(row-height icons) with **every near-read symbol** —
`janus_font_glyph_*`, all `*_widgets` / `*_arr*` descriptor arrays, all
`*_str*` flash strings, `janus_app_screens` — at VMA `< 0x10000`. Image
`_px` arrays stay above it (read far, per task 2). Host suite + `ctest`
unchanged.

### Proposed finalized contract (post-spike — for Rafael's OK)

**Approach: "link image arrays last" + a generation-time budget guard.**
No runtime read changes; no consumer build-config edit.

Delivered:

- **`janus/stage3b_embedded_c/image_asset.py` / `emit_embedded_c.py`
  (`_image_fields_c`)** — the baked `_px` arrays are emitted **without**
  `JANUS_PROGMEM`, with `JANUS_IMG_SECTION` instead (new macro).
- **`runtime/embedded_c/include/janus_progmem.h`** — `JANUS_IMG_SECTION`:
  on AVR `__attribute__((used, section(".janus_img")))`, off-AVR empty
  (or `JANUS_PROGMEM` — behaviourally identical on host). Doc note: the
  section is deliberately *not* `__progmem__` (avr-gcc ignores `section`
  when it is); reads stay correct via task 2's `JANUS_FAR_ADDR` +
  `memcpy_PF`, and nothing near-derefs `_px`.
- **`runtime/embedded_c/janus_img.ld`** (new) — the one-liner
  `SECTIONS { .janus_img : { *(.janus_img*) } } INSERT AFTER .text;`,
  vendored with the rest of the runtime.
- **`runtime/embedded_c/CMakeLists.txt`** —
  `target_link_options(janus_runtime INTERFACE -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/janus_img.ld)`
  so a CMake consumer's link gets the fragment with no edit. Non-CMake
  builds rely on avr-ld orphan placement (spike-verified) and can add
  `-Wl,-T .../janus_img.ld` by hand if a real board ever needs it —
  documented in the scaffold README + `Janus.md`.
- **Generation-time budget guard** — `image_asset` / a Stage 3b check
  sums baked-image bytes across all screens; `log.warning` (not fatal —
  matches the existing per-image warn) if the non-image near-`.progmem`
  estimate would still plausibly cross ~56 KiB. Legibility backstop only.
- **`scripts/avr_gate.sh`** — extend step 4 with the `avr-nm` VMA
  assertion: every near-read symbol (`janus_font_glyph_*`, `*_widgets`,
  `*_arr*`, `*_str*` that isn't `*_px`, `janus_app_screens`, nav titles)
  must be `< 0x10000`; `_px` arrays may be anywhere.
- **Docs** — `architecture.md` Stage 3b (image section + placement) and
  Stage 4 (why `_px` isn't `PROGMEM`); `Janus.md` RGB565/PROGMEM section.

Dropped from the original candidate list (spike showed unnecessary):
far-safing the font reads, far-safing the descriptor graph.

## Dependencies

Task 2 (the image far-read path it builds on). Independent of task 3.

## Pre-work

### Research spike — DONE (2026-09-06). Outcome: **"link image arrays last" is viable with no consumer build-config edit.**

Reproduced on the real `examples/host_demo` with `avr-gcc -mmcu=atmega2560
-Os` (avr-gcc 7.3 / avr-ld 2.26). Baseline: `janus_font_glyph_medium/large`
link at `0x19b2c`, `relay_widgets` at `0x19322`, most `pwm_strNN` flash
strings at `0x15xxx–0x18xxx` — all **> 0x10000**, all read `near` → the
bug.

What was tried:

1. **`__attribute__((section("…")))` alongside `JANUS_PROGMEM`** — avr-gcc
   *silently ignores* the `section` attribute when `__progmem__` is also
   on the symbol; the array stays in `.progmem.data`. Also tried
   `__attribute__((__progmem__, section("…")))` in one list — same, ignored.
   So a custom section is **incompatible with `PROGMEM`** on this toolchain.
2. **Link order** (font/runtime objects first, screen `.gen.o` last) — no
   effect: the `_px` arrays and the `pwm_strNN` strings are defined in the
   *same* object, same input section, so source order inside
   `*_screen.gen.c` (arrays emitted first) is what pushes the strings up.
3. **Drop `PROGMEM`, use `__attribute__((used, section(".janus_img")))`**
   — works. avr-gcc keeps the arrays **flash-only, no RAM shadow**
   (verified: `.data` 258 B / `.bss` 1318 B with 84 000 B of images in
   `.janus_img`). Reads are unaffected because task 2 already routes image
   pixels through `JANUS_FAR_ADDR` (`pgm_get_far_address`) + `memcpy_PF`,
   which need no `__progmem__` on the symbol, and nothing ever near-derefs
   `_px` (the descriptor carries `image_slot`, an int, not a pointer).
4. **Placement of `.janus_img`:**
   - *Bare link, no linker script at all:* avr-ld's **orphan-section
     placement puts `.janus_img` immediately after `.text`** on its own —
     `janus_font_glyph_*` back down at `0x530c`, every descriptor / string
     / `janus_app_screens` **< 0x10000**, `_px` arrays at `0x6d16+`. Zero
     consumer config. Not *contractually* guaranteed by ld, but it is the
     documented orphan heuristic (a read-only alloc section trails `.text`)
     and was stable here.
   - *Explicit:* a one-line fragment
     `SECTIONS { .janus_img : { *(.janus_img*) } } INSERT AFTER .text;`
     via `-Wl,-T,<frag>` — same result, now guaranteed. Shippable in the
     vendored runtime and wired through the vendored `CMakeLists.txt`
     (`target_link_options(janus_runtime INTERFACE -Wl,-T,<frag>)`, which
     propagates to the consumer's link) — **still no consumer edit** for a
     CMake project.
   - *PlatformIO `atmelavr`:* not installed on the spike box, not directly
     tested. Falls back to the bare/orphan case (works); if a future
     real-hardware test shows orphan placement failing there, PlatformIO
     libraries can ship an `extra_scripts` hook in `library.json` that
     appends `LINKFLAGS` — reaches the link, still shippable in the
     vendored runtime, just more machinery. Document a one-line
     `build_flags` fallback as the last resort.

Result after the fix, full `host_demo`: **every symbol ≥ 0x10000 is a
`_px` image array or a RAM (`0x80xxxx`) symbol** — no font, descriptor,
string, or screen-table above the near window.

Font-far-safing and descriptor-graph-far-safing are **not needed** and
drop off the candidate list.

## Tests

- A script (`ctest` fixture or standalone) that `avr-nm`s the linked
  `host_demo` `.elf` and asserts the near-read symbol set is all
  `< 0x10000`.
- If font reads change: `test_font.c` stays green on host; add an
  AVR-only note/verification that the far path is taken.
- `avr-gcc -mmcu=atmega2560` link of `host_demo` — the concrete gate,
  combined with task 3's SRAM gate = the **epic acceptance gate**.

## DoD

Spike done + approach chosen + this file finalized · contract delivered ·
Python suite green · `ctest` green · the `avr-nm` VMA assertion passes ·
docs updated (`architecture.md` Stage 3b/4) · this file marked done ·
commit + merge `4-near-progmem-budget → tasks`.
