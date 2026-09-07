/* janus_format.c tests — the allocation-free label/header format-template
 * substitution. No driver involved (janus_format.c only reaches into
 * janus_bound.c for the value read), so like test_font.c this needs no
 * mock driver.
 *
 * The value always arrives through a janus_bind_t + a bound struct
 * instance, exactly as a generated screen would hand it over — so the
 * fixtures below are a small struct with one field per bind type and
 * binds built with offsetof() against it.
 *
 * Float cases use values that are *exact* in IEEE-754 (halves, eighths)
 * so the assertions pin the formatter's own rounding rule
 * (round-half-away-from-zero, carry rippling into the whole part) rather
 * than the representation error of an arbitrary decimal — a `type: float`
 * bind stores 32-bit float, so e.g. 0.005 could never round-trip to test
 * "half rounds up" anyway.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "janus_format.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

typedef struct {
    int i;
    int64_t i64;
    float f;
    const char *s;
} tv_t;

static janus_bind_t bind_of(janus_field_type_t t, size_t off) {
    janus_bind_t b;
    memset(&b, 0, sizeof b);
    b.field_type = t;
    b.field_offset = (uint16_t)off;
    return b;
}

/* Format into a shared scratch buffer, pre-poisoned so a stray write is
 * visible, and return it for string comparison. */
static char g_out[128];
static const char *F(const char *fmt, const janus_bind_t *b, const void *bs) {
    memset(g_out, '#', sizeof g_out);
    janus_format_into(g_out, sizeof g_out, fmt, b, bs);
    return g_out;
}

#define CHECK_STR(actual, expected) do { \
    const char *a_ = (actual); \
    if (strcmp(a_, (expected)) != 0) { \
        g_failures++; \
        fprintf(stderr, "FAIL: \"%s\" != \"%s\" (%s:%d)\n", a_, (expected), __FILE__, __LINE__); \
    } \
} while (0)

static void test_integers(void) {
    tv_t v = {0};
    janus_bind_t bi = bind_of(JANUS_FIELD_INT, offsetof(tv_t, i));

    v.i = 72;   CHECK_STR(F("%d", &bi, &v), "72");
    v.i = -5;   CHECK_STR(F("%d", &bi, &v), "-5");
    v.i = 0;    CHECK_STR(F("%d", &bi, &v), "0");

    /* %% and surrounding literals */
    v.i = 72;   CHECK_STR(F("Duty: %d%%", &bi, &v), "Duty: 72%");
}

static void test_unsigned_and_hex(void) {
    tv_t v = {0};
    janus_bind_t bi = bind_of(JANUS_FIELD_INT, offsetof(tv_t, i));

    /* read path is double: (int64_t)(double)(-1) == -1, then a 32-bit
     * unsigned view — pinned, not left implementation-defined */
    v.i = -1;   CHECK_STR(F("%u", &bi, &v), "4294967295");
    v.i = 255;  CHECK_STR(F("%x", &bi, &v), "ff");
    v.i = -1;   CHECK_STR(F("%x", &bi, &v), "ffffffff");
    v.i = 0;    CHECK_STR(F("%u", &bi, &v), "0");
}

static void test_long_forms_are_64_bit(void) {
    tv_t v = {0};
    janus_bind_t bl = bind_of(JANUS_FIELD_INT64, offsetof(tv_t, i64));

    /* within double's integer-exact range (< 2^53) — round-trips exactly.
     * Past that it is lossy by construction (value is read as double). */
    v.i64 = 1234567890123LL;   CHECK_STR(F("%lld", &bl, &v), "1234567890123");
    v.i64 = -1234567890123LL;  CHECK_STR(F("%ld", &bl, &v), "-1234567890123");
}

static void test_fixed_point(void) {
    tv_t v = {0};
    janus_bind_t bf = bind_of(JANUS_FIELD_FLOAT, offsetof(tv_t, f));

    v.f = 21.5f;   CHECK_STR(F("%.1f", &bf, &v), "21.5");
    v.f = 2.5f;    CHECK_STR(F("%.0f", &bf, &v), "3");      /* half rounds away from zero */
    v.f = 0.125f;  CHECK_STR(F("%.2f", &bf, &v), "0.13");   /* .125 -> .13 */
    v.f = -0.125f; CHECK_STR(F("%.2f", &bf, &v), "-0.13");  /* sign kept, half away */
    v.f = 9.999f;  CHECK_STR(F("%.2f", &bf, &v), "10.00");  /* carry ripples into whole */
    v.f = 3.0f;    CHECK_STR(F("%f", &bf, &v), "3.00");     /* default precision is 2 */
    v.f = 1.5f;    CHECK_STR(F("%.3f", &bf, &v), "1.500");  /* zero-pad to N */
}

static void test_string(void) {
    tv_t v = {0};
    janus_bind_t bs = bind_of(JANUS_FIELD_STRING, offsetof(tv_t, s));

    v.s = "hello";  CHECK_STR(F(">%s<", &bs, &v), ">hello<");
    v.s = NULL;     CHECK_STR(F(">%s<", &bs, &v), ">(null)<");
}

static void test_no_arg_passes_specs_through(void) {
    janus_bind_t none = bind_of(JANUS_FIELD_NONE, 0);

    CHECK_STR(F("x=%d", &none, NULL), "x=%d");
    CHECK_STR(F("x=%d", NULL, NULL), "x=%d");   /* NULL bind, not just NONE */
    CHECK_STR(F("100%%", &none, NULL), "100%");  /* %% still collapses */
    CHECK_STR(F("done%", &none, NULL), "done%"); /* trailing bare % */
}

static void test_second_conversion_is_literal(void) {
    tv_t v = {0};
    v.i = 72;
    janus_bind_t bi = bind_of(JANUS_FIELD_INT, offsetof(tv_t, i));

    CHECK_STR(F("%d %d", &bi, &v), "72 %d");
}

static void test_unknown_conversion_is_literal(void) {
    tv_t v = {0};
    v.i = 72;
    janus_bind_t bi = bind_of(JANUS_FIELD_INT, offsetof(tv_t, i));

    CHECK_STR(F("%q", &bi, &v), "%q");
    CHECK_STR(F("plain text", &bi, &v), "plain text");
}

static void test_cap_clamps_and_never_overruns(void) {
    char buf[8];
    memset(buf, '#', sizeof buf);
    tv_t v = {0};
    v.i = 12345;
    janus_bind_t bi = bind_of(JANUS_FIELD_INT, offsetof(tv_t, i));

    size_t r = janus_format_into(buf, 4, "%d", &bi, &v);
    CHECK(r == 3);
    CHECK(strcmp(buf, "123") == 0);
    CHECK(buf[3] == '\0');
    CHECK(buf[4] == '#');   /* nothing written at or past cap */
    CHECK(buf[5] == '#');

    /* cap == 1: only the NUL fits */
    char one[1] = { '#' };
    r = janus_format_into(one, 1, "%d", &bi, &v);
    CHECK(r == 0);
    CHECK(one[0] == '\0');

    /* cap == 0: must not touch the buffer at all */
    char zero[1] = { '#' };
    r = janus_format_into(zero, 0, "%d", &bi, &v);
    CHECK(r == 0);
    CHECK(zero[0] == '#');
}

int main(void) {
    test_integers();
    test_unsigned_and_hex();
    test_long_forms_are_64_bit();
    test_fixed_point();
    test_string();
    test_no_arg_passes_specs_through();
    test_second_conversion_is_literal();
    test_unknown_conversion_is_literal();
    test_cap_clamps_and_never_overruns();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
