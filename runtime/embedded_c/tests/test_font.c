/* janus_font.c tests — the v1 5x7 glyph table (space + 'A'-'Z') is pure
 * data + lookup, no driver involved, so this doesn't need the mock
 * driver the way test_runtime.c/test_input_touch.c do. */
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

static void test_lowercase_case_folds_onto_uppercase(void) {
    /* v1: no distinct lowercase glyphs — 'a' renders as 'A', not blank. */
    CHECK(janus_font_glyph('a') == janus_font_glyph('A'));
    CHECK(janus_font_glyph('z') == janus_font_glyph('Z'));
}

static void test_digits_and_punctuation_have_no_glyph_yet(void) {
    /* Deliberate v1 gap, not an oversight — see janus_font.h. */
    CHECK(janus_font_glyph('0') == NULL);
    CHECK(janus_font_glyph('%') == NULL);
    CHECK(janus_font_glyph('.') == NULL);
}

static void test_every_letter_has_a_distinct_bitmap(void) {
    /* Catches the easy transcription mistake of two letters sharing one
     * accidentally-duplicated row in the source table. */
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
    for (char c = 'A'; c <= 'Z'; c++) {
        const uint8_t *g = janus_font_glyph(c);
        for (int i = 0; i < JANUS_FONT_GLYPH_W; i++) {
            CHECK((g[i] & 0x80) == 0);
        }
    }
}

int main(void) {
    test_glyph_dimensions_are_5x7();
    test_space_and_uppercase_have_glyphs();
    test_lowercase_case_folds_onto_uppercase();
    test_digits_and_punctuation_have_no_glyph_yet();
    test_every_letter_has_a_distinct_bitmap();
    test_no_glyph_bit_past_row_6_is_set();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
