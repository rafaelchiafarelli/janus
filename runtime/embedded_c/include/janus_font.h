/* Janus embedded-C runtime — 5x7 bitmap font. Stage 4 glyph rendering
 * (see architecture.md): used for both authored `text:` and live bound
 * string values (janus_runtime.c's draw_label/draw_header) — this module
 * only supplies glyph lookup, agnostic to where the string came from.
 *
 * Occidental (Latin) coverage: space, digits, common punctuation, true
 * upper/lowercase letters (no case-folding — 'a' and 'A' are distinct
 * glyphs), and the Latin-1 accented set needed for Western European
 * languages (á é í ó ú ñ ü ç à è ì ò ù â ê î ô û ã õ and uppercase
 * equivalents). At 5x7, an accent mark is a coarse one-row indicator
 * distinguished by which column(s) it lights, not faithful stroke shape.
 * A handful of ASCII punctuation marks still have no glyph (`< > [ ] \ ^
 * \` { | } ~ $`) — not required by anything Janus itself authors or
 * generates today; widening further is pure data, same as before.
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

#define JANUS_FONT_GLYPH_W 5
#define JANUS_FONT_GLYPH_H 7

/* Returns the glyph's 5 column-bytes (bit r = row r, 0 = top, 6 =
 * bottom; set = lit pixel), or NULL if `c` has no glyph in this font. */
const uint8_t *janus_font_glyph(char c);

#endif /* JANUS_FONT_H */
