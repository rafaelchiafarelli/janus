/* Janus embedded-C runtime — bitmap fonts. Stage 4 glyph rendering (see
 * architecture.md): used for both authored `text:` and live bound string
 * values (janus_runtime.c's draw_label/draw_header) — this module only
 * supplies glyph lookup, agnostic to where the string came from.
 *
 * Two fixed sizes, not one: `medium` (10x14) and `large` (20x28) —
 * `large`'s original size, `medium` a 2x2 box-filter downsample of the
 * exact same glyphs (not an independently-hinted small render), so the
 * two sizes are faithfully proportional to each other rather than two
 * unrelated designs. A widget picks one via `janus_widget_desc_t.font_size`
 * and can further multiply it with an integer `.font_scale` (pixel
 * replication in janus_runtime.c's draw_glyph) — capped so the scaled
 * result never exceeds `large`'s own 20x28 footprint (that's what sizes
 * janus_runtime.c's glyph scratch buffer, a fixed ~2 KiB-budget buffer,
 * no malloc; see Janus.md), which in practice means `large` only takes
 * scale 1 and `medium` takes 1 or 2 (2x medium == large's own size, one
 * of the two ways to reach that footprint). Stage 1 (dsl_yaml.py)
 * enforces the cap at parse time; this runtime doesn't re-check it, so a
 * hand-built descriptor that skips validation and authors a scale too
 * large gets its glyph silently clamped back down instead of corrupting
 * the buffer (see draw_string). There's deliberately no arbitrary-divisor
 * downscale: shrinking a 1-bit glyph by an arbitrary ratio drops strokes
 * unpredictably (see the commit that added `medium` for the fuller
 * argument), so going smaller than `medium` means adding a third real
 * table, not dividing one of these.
 *
 * Occidental (Latin) coverage: space, digits, common punctuation, true
 * upper/lowercase letters (no case-folding — 'a' and 'A' are distinct
 * glyphs), and the Latin-1 accented set needed for Western European
 * languages (á é í ó ú ñ ü ç à è ì ò ù â ê î ô û ã õ and uppercase
 * equivalents). Both tables rasterized from DejaVu Sans Mono Bold at
 * 24pt (one-off script, not shipped — see the commit that widened this
 * table from the original 5x7 hand-authored set). A handful of ASCII
 * punctuation marks still have no glyph (`< > [ ] \ ^ \` { | } ~ $`) —
 * not required by anything Janus itself authors or generates today;
 * widening further is pure data, same as before.
 *
 * **Strings must be Latin-1 (ISO-8859-1) encoded, not UTF-8.** This
 * module maps one `char` to one glyph — a UTF-8 accented character is
 * multiple bytes and would render as mojibake (each byte looked up and
 * blitted independently), not decoded as one codepoint. Latin-1 encodes
 * every accented character in this font as a single byte, matching the
 * one-`char`-per-`draw_glyph` call in `janus_runtime.c`'s `draw_string`.
 */
#ifndef JANUS_FONT_H
#define JANUS_FONT_H

#include <stdint.h>

#define JANUS_FONT_MEDIUM_GLYPH_W 10
#define JANUS_FONT_MEDIUM_GLYPH_H 14
/* Bytes per column: rows packed 8-per-byte, rounded up — top 2 bits of
 * the last byte are unused padding. */
#define JANUS_FONT_MEDIUM_GLYPH_COL_BYTES 2

#define JANUS_FONT_LARGE_GLYPH_W 20
#define JANUS_FONT_LARGE_GLYPH_H 28
/* Bytes per column: rows packed 8-per-byte, rounded up — top 4 bits of
 * the last byte are unused padding. */
#define JANUS_FONT_LARGE_GLYPH_COL_BYTES 4

/* `janus_widget_desc_t.font_size` (janus_runtime.h) — which table a
 * widget's text renders from. LARGE == 0 deliberately: a zero-initialized
 * (or simply `.font_size` omitted) widget desc renders at `large`, the
 * only size this runtime had before `medium` existed — every generated
 * or hand-built descriptor that predates this field keeps behaving
 * exactly as before with no migration. */
typedef enum {
    JANUS_FONT_SIZE_LARGE = 0,
    JANUS_FONT_SIZE_MEDIUM = 1,
} janus_font_size_t;

/* Returns the glyph's JANUS_FONT_MEDIUM_GLYPH_W * _COL_BYTES bytes
 * (column-major: column c's rows live at glyph[c * COL_BYTES + row / 8],
 * bit (row % 8), row 0 = top; set = lit pixel), or NULL if `c` has no
 * glyph in this font. Flash-resident (JANUS_PROGMEM) on AVR — read each
 * byte via JANUS_PGM_READ_U8 (janus_progmem.h), never a plain
 * dereference; see janus_runtime.c's draw_glyph. */
const uint8_t *janus_font_glyph_medium(char c);

/* Same contract as janus_font_glyph_medium, for the 20x28 table. */
const uint8_t *janus_font_glyph_large(char c);

#endif /* JANUS_FONT_H */
