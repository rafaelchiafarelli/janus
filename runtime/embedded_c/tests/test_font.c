/* janus_font.c tests — the occidental medium (10x14) and large (20x28)
 * glyph tables (space, digits, punctuation, upper/lowercase letters,
 * Latin-1 accents) are pure data + lookup, no driver involved, so this
 * doesn't need the mock driver the way test_runtime.c/test_input_touch.c
 * do. Both tables share the same char coverage (medium is a downsample
 * of large, not an independently-authored set — janus_font.h), so most
 * checks below run against both via a glyph-fn parameter instead of
 * being duplicated per size; only the byte-layout checks (which need
 * each table's own W/H/col_bytes) are written out per size. */
#include <stdio.h>

#include "janus_font.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

typedef const uint8_t *(*font_glyph_fn)(char);

static void check_space_and_uppercase_have_glyphs(font_glyph_fn glyph) {
    CHECK(glyph(' ') != NULL);
    for (char c = 'A'; c <= 'Z'; c++) {
        CHECK(glyph(c) != NULL);
    }
}

static void check_lowercase_has_true_distinct_glyphs(font_glyph_fn glyph, int glyph_bytes) {
    /* No case-folding: 'a' is its own glyph, not a copy of 'A'. */
    for (char c = 'a'; c <= 'z'; c++) {
        const uint8_t *lower = glyph(c);
        const uint8_t *upper = glyph((char)(c - 'a' + 'A'));
        CHECK(lower != NULL);
        CHECK(upper != NULL);
        int same = 1;
        for (int i = 0; i < glyph_bytes; i++) {
            if (lower[i] != upper[i]) { same = 0; break; }
        }
        CHECK(!same);
    }
}

static void check_digits_have_glyphs(font_glyph_fn glyph) {
    for (char c = '0'; c <= '9'; c++) {
        CHECK(glyph(c) != NULL);
    }
}

static void check_representative_punctuation_has_glyphs(font_glyph_fn glyph) {
    const char *punct = ".,!?:;'\"-()/+=@&%#*_";
    for (const char *p = punct; *p != '\0'; p++) {
        CHECK(glyph(*p) != NULL);
    }
}

static void check_accented_latin1_characters_have_glyphs(font_glyph_fn glyph) {
    /* Latin-1 byte values (see janus_font.h — strings are Latin-1, not
     * UTF-8), a representative subset covering every accent this font
     * distinguishes: acute, grave, circumflex, tilde, diaeresis, cedilla. */
    const unsigned char accented[] = {
        0xc1, 0xc0, 0xc2, 0xc3, 0xdc, 0xc7,  /* A-acute/grave/circ/tilde, U-diaeresis, C-cedilla */
        0xe1, 0xe8, 0xea, 0xf5, 0xfc, 0xe7,  /* lowercase equivalents */
    };
    for (size_t i = 0; i < sizeof(accented) / sizeof(accented[0]); i++) {
        CHECK(glyph((char)accented[i]) != NULL);
    }
}

static void check_unmapped_characters_have_no_glyph(font_glyph_fn glyph) {
    /* Not required by anything Janus authors/generates today — real gaps,
     * not oversights (see janus_font.h). */
    CHECK(glyph('<') == NULL);
    CHECK(glyph('$') == NULL);
    CHECK(glyph('~') == NULL);
}

static void check_every_letter_has_a_distinct_bitmap(font_glyph_fn glyph, int glyph_bytes) {
    /* Catches the easy transcription mistake of two letters sharing one
     * accidentally-duplicated row in the source table. Restricted to
     * uppercase A-Z — the full 123-glyph table's distinctness was
     * verified at authoring time by the (unshipped) generator script
     * instead of re-checked here. */
    for (char a = 'A'; a <= 'Z'; a++) {
        for (char b = (char)(a + 1); b <= 'Z'; b++) {
            const uint8_t *ga = glyph(a);
            const uint8_t *gb = glyph(b);
            int same = 1;
            for (int i = 0; i < glyph_bytes; i++) {
                if (ga[i] != gb[i]) { same = 0; break; }
            }
            CHECK(!same);
        }
    }
}

static void check_no_glyph_bit_past_last_row_is_set(font_glyph_fn glyph, int w, int h, int col_bytes) {
    /* Each column packs `h` rows into `col_bytes` bytes, 8 rows per byte —
     * the last byte's high bits (beyond `h % 8` valid ones, or all 8 if
     * `h` is a multiple of 8) are padding that must stay zero or
     * draw_glyph's row loop would read a nonexistent row past `h` as lit. */
    const char *sample = "AZaz09.@";
    const int last_byte_valid_bits = (h % 8 == 0) ? 8 : (h % 8);
    const uint8_t padding_mask = (uint8_t)(last_byte_valid_bits == 8 ? 0 : (0xFF << last_byte_valid_bits));
    for (const char *p = sample; *p != '\0'; p++) {
        const uint8_t *g = glyph(*p);
        CHECK(g != NULL);
        for (int col = 0; col < w; col++) {
            uint8_t last_byte = g[col * col_bytes + col_bytes - 1];
            CHECK((last_byte & padding_mask) == 0);
        }
    }
}

static void test_medium_dimensions_are_10x14(void) {
    CHECK(JANUS_FONT_MEDIUM_GLYPH_W == 10);
    CHECK(JANUS_FONT_MEDIUM_GLYPH_H == 14);
    CHECK(JANUS_FONT_MEDIUM_GLYPH_COL_BYTES == 2);
}

static void test_large_dimensions_are_20x28(void) {
    CHECK(JANUS_FONT_LARGE_GLYPH_W == 20);
    CHECK(JANUS_FONT_LARGE_GLYPH_H == 28);
    CHECK(JANUS_FONT_LARGE_GLYPH_COL_BYTES == 4);
}

int main(void) {
    test_medium_dimensions_are_10x14();
    test_large_dimensions_are_20x28();

    const int medium_bytes = JANUS_FONT_MEDIUM_GLYPH_W * JANUS_FONT_MEDIUM_GLYPH_COL_BYTES;
    check_space_and_uppercase_have_glyphs(janus_font_glyph_medium);
    check_lowercase_has_true_distinct_glyphs(janus_font_glyph_medium, medium_bytes);
    check_digits_have_glyphs(janus_font_glyph_medium);
    check_representative_punctuation_has_glyphs(janus_font_glyph_medium);
    check_accented_latin1_characters_have_glyphs(janus_font_glyph_medium);
    check_unmapped_characters_have_no_glyph(janus_font_glyph_medium);
    check_every_letter_has_a_distinct_bitmap(janus_font_glyph_medium, medium_bytes);
    check_no_glyph_bit_past_last_row_is_set(janus_font_glyph_medium,
        JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H, JANUS_FONT_MEDIUM_GLYPH_COL_BYTES);

    const int large_bytes = JANUS_FONT_LARGE_GLYPH_W * JANUS_FONT_LARGE_GLYPH_COL_BYTES;
    check_space_and_uppercase_have_glyphs(janus_font_glyph_large);
    check_lowercase_has_true_distinct_glyphs(janus_font_glyph_large, large_bytes);
    check_digits_have_glyphs(janus_font_glyph_large);
    check_representative_punctuation_has_glyphs(janus_font_glyph_large);
    check_accented_latin1_characters_have_glyphs(janus_font_glyph_large);
    check_unmapped_characters_have_no_glyph(janus_font_glyph_large);
    check_every_letter_has_a_distinct_bitmap(janus_font_glyph_large, large_bytes);
    check_no_glyph_bit_past_last_row_is_set(janus_font_glyph_large,
        JANUS_FONT_LARGE_GLYPH_W, JANUS_FONT_LARGE_GLYPH_H, JANUS_FONT_LARGE_GLYPH_COL_BYTES);

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
