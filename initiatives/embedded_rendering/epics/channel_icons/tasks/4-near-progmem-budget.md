# Task 4: near-progmem-budget

Status: **scoped — needs a research spike first (see Pre-work)**.

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

### Candidate approaches (pick during scoping, after the spike)

- **Link image arrays last.** Give the baked `_px` arrays their own
  section (`__attribute__((section(".janus_img_progmem")))` in
  `_image_fields_c`) and force it to the top of flash — via a linker
  fragment `INSERT AFTER .text` shipped in the vendored runtime, or
  `-Wl,--section-start=.janus_img_progmem=0x…`. Everything else then
  stays low and near-safe with **no runtime read changes**. The open
  question is whether the placement can be applied *without* a consumer
  build-config edit (see spike).
- **Far-safe the font reads.** Make `janus_font_glyph_*` return a
  far-qualified pointer (`__memx`, or a resolved-once far address like
  the image table) and switch the font byte reads to `memcpy_PF` /
  far loads. Smaller than "far-everything" because the font tables are
  self-contained; does not protect descriptor arrays / strings if those
  later cross 64 KiB too.
- **Far-safe the whole descriptor graph.** Correct but large — the
  `janus_screen_desc_t.widgets` / `.children[i]` pointers and every
  `pgm_read_*` in the traversal. Last resort.
- **Generation-time budget guard.** Sum baked-image bytes; error (or
  loud-warn) if the total plausibly pushes near-`.progmem` over ~56 KiB.
  Doesn't fix anything — only fails legibly. Pairs with one of the above.

## Dependencies

Task 2 (the image far-read path it builds on). Independent of task 3.

## Pre-work

**Research spike (its own thing — flag to Rafael, may need a throwaway
build, ≤ half a session):** determine whether a vendored linker fragment
or a fixed section address can place `.janus_img_progmem` at high flash
**without** the consuming project adding `-Wl,-T` / `board_build.ldscript`
/ `build_flags`. Check PlatformIO's `atmelavr` platform specifically
(does `lib_deps = lib/GUI/runtime` honor a `.ld` the library ships? does
`extraScript` reach the link?). The spike's output is: which "candidate
approach" above is actually viable → then this task can be finalized and
implemented. If the spike shows every placement route needs a consumer
edit, the decision becomes "accept one `-D`/`-Wl` line in consumer
builds" vs "far-safe the font reads" — Rafael's call.

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
