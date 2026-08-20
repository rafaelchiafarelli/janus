/* Janus embedded-C runtime — 5x7 bitmap font. Stage 4 glyph-rendering
 * slice 1 of 2 (see architecture.md): static `text:` only. Bound string
 * fields still don't render — that's slice 2, a separate follow-up.
 *
 * Deliberately minimal coverage: space + 'A'-'Z' (27 glyphs) — enough
 * for this project's own authored strings ("Device Status", "Reboot",
 * "Diagnostics"). No digits/punctuation/lowercase glyphs yet;
 * lowercase input case-folds onto the uppercase glyph instead of going
 * unrendered. Widening the character set is pure data (add rows to
 * GLYPHS in janus_font.c), never touches the blit logic in
 * janus_runtime.c.
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
