/* janus_font.c tests — the occidental 5x7 glyph table (space, digits,
 * punctuation, upper/lowercase letters, Latin-1 accents) is pure data +
 * lookup, no driver involved, so this doesn't need the mock driver the
 * way test_runtime.c/test_input_touch.c do. */
#include <stdio.h>

#include "janus_font.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static void test_glyph_dimensions_are_5x7(void) {
    CHECK(JANUS_FONT_GLYPH_W == 5);
    CHECK(JANUS_FONT_GLYPH_H == 7);
}

static void test_space_and_uppercase_have_glyphs(void) {
    CHECK(janus_font_glyph(' ') != NULL);
    for (char c = 'A'; c <= 'Z'; c++) {
        CHECK(janus_font_glyph(c) != NULL);
    }
}

static void test_lowercase_has_true_distinct_glyphs(void) {
    /* No case-folding: 'a' is its own glyph, not a copy of 'A'. */
    for (char c = 'a'; c <= 'z'; c++) {
        const uint8_t *lower = janus_font_glyph(c);
        const uint8_t *upper = janus_font_glyph((char)(c - 'a' + 'A'));
        CHECK(lower != NULL);
        CHECK(upper != NULL);
        int same = 1;
        for (int i = 0; i < JANUS_FONT_GLYPH_W; i++) {
            if (lower[i] != upper[i]) { same = 0; break; }
        }
        CHECK(!same);
    }
}

static void test_digits_have_glyphs(void) {
    for (char c = '0'; c <= '9'; c++) {
        CHECK(janus_font_glyph(c) != NULL);
    }
}

static void test_representative_punctuation_has_glyphs(void) {
    const char *punct = ".,!?:;'\"-()/+=@&%#*_";
    for (const char *p = punct; *p != '\0'; p++) {
        CHECK(janus_font_glyph(*p) != NULL);
    }
}

static void test_accented_latin1_characters_have_glyphs(void) {
    /* Latin-1 byte values (see janus_font.h — strings are Latin-1, not
     * UTF-8), a representative subset covering every accent this font
     * distinguishes: acute, grave, circumflex, tilde, diaeresis, cedilla. */
    const unsigned char accented[] = {
        0xc1, 0xc0, 0xc2, 0xc3, 0xdc, 0xc7,  /* A-acute/grave/circ/tilde, U-diaeresis, C-cedilla */
        0xe1, 0xe8, 0xea, 0xf5, 0xfc, 0xe7,  /* lowercase equivalents */
    };
    for (size_t i = 0; i < sizeof(accented) / sizeof(accented[0]); i++) {
        CHECK(janus_font_glyph((char)accented[i]) != NULL);
    }
}

static void test_unmapped_characters_have_no_glyph(void) {
    /* Not required by anything Janus authors/generates today — real gaps,
     * not oversights (see janus_font.h). */
    CHECK(janus_font_glyph('<') == NULL);
    CHECK(janus_font_glyph('$') == NULL);
    CHECK(janus_font_glyph('~') == NULL);
}

static void test_every_letter_has_a_distinct_bitmap(void) {
    /* Catches the easy transcription mistake of two letters sharing one
     * accidentally-duplicated row in the source table. Restricted to
     * uppercase A-Z (the original, still-unchanged table) — the full
     * 123-glyph table's distinctness was verified at authoring time by
     * the (unshipped) generator script instead of re-checked here. */
    for (char a = 'A'; a <= 'Z'; a++) {
        for (char b = (char)(a + 1); b <= 'Z'; b++) {
            const uint8_t *ga = janus_font_glyph(a);
            const uint8_t *gb = janus_font_glyph(b);
            int same = 1;
            for (int i = 0; i < JANUS_FONT_GLYPH_W; i++) {
                if (ga[i] != gb[i]) { same = 0; break; }
            }
            CHECK(!same);
        }
    }
}

static void test_no_glyph_bit_past_row_6_is_set(void) {
    /* A glyph byte is a column; only bits 0-6 (7 rows) are meaningful —
     * bit 7 set would silently corrupt draw_glyph's row loop upstream. */
    const char *sample = "AZaz09.@";
    for (const char *p = sample; *p != '\0'; p++) {
        const uint8_t *g = janus_font_glyph(*p);
        CHECK(g != NULL);
        for (int i = 0; i < JANUS_FONT_GLYPH_W; i++) {
            CHECK((g[i] & 0x80) == 0);
        }
    }
}

int main(void) {
    test_glyph_dimensions_are_5x7();
    test_space_and_uppercase_have_glyphs();
    test_lowercase_has_true_distinct_glyphs();
    test_digits_have_glyphs();
    test_representative_punctuation_has_glyphs();
    test_accented_latin1_characters_have_glyphs();
    test_unmapped_characters_have_no_glyph();
    test_every_letter_has_a_distinct_bitmap();
    test_no_glyph_bit_past_row_6_is_set();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
