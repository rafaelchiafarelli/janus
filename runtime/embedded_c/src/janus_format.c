/* Janus embedded-C runtime — label/header format templates. See
 * janus_format.h.
 *
 * Hand-rolled so a `%f` label doesn't drag avr-libc's ~1-2 KiB
 * vfprintf into a firmware image. The bound value is read as a `double`
 * (janus_bound.c) — the render module already does double math for
 * progress/gauge ranges, so soft-float is linked regardless; this file
 * adds only the digit extraction and the fixed-point split.
 */
#include "janus_format.h"

#include <stdbool.h>
#include <stdint.h>

#include "janus_bound.h"
#include "janus_progmem.h"

/* ------------------------------------------------------------ output --
 * Bounded writer: stops at cap-1 so there is always room for the NUL,
 * and `out` only advances when a byte is actually stored — so the return
 * value is the real (clamped) length, never the would-be length. */
typedef struct {
    char *buf;
    size_t cap;
    size_t out;
} fmtctx;

static void put_ch(fmtctx *c, char ch) {
    if (c->cap == 0) return;
    if (c->out < c->cap - 1) c->buf[c->out++] = ch;
}

static void put_ram(fmtctx *c, const char *s) {
    for (; *s != '\0'; s++) put_ch(c, *s);
}

/* --------------------------------------------------------- fmt reads --
 * `fmt` is flash on AVR (PROGMEM) — never a plain deref. */
static char fmt_at(const char *fmt, size_t i) {
    return (char)JANUS_PGM_READ_U8((const uint8_t *)fmt + i);
}

static void emit_flash_span(fmtctx *c, const char *fmt, size_t from, size_t to_incl) {
    for (size_t k = from; k <= to_incl; k++) {
        char ch = fmt_at(fmt, k);
        if (ch == '\0') break;
        put_ch(c, ch);
    }
}

/* ---------------------------------------------------- integer digits --
 */
static void emit_u64_dec(fmtctx *c, uint64_t v) {
    char rev[20];
    int n = 0;
    do {
        rev[n++] = (char)('0' + (unsigned)(v % 10u));
        v /= 10u;
    } while (v != 0 && n < 20);
    while (n > 0) put_ch(c, rev[--n]);
}

static void emit_u64_hex(fmtctx *c, uint64_t v) {
    char rev[16];
    int n = 0;
    do {
        unsigned d = (unsigned)(v & 0xFu);
        rev[n++] = (char)(d < 10u ? '0' + d : 'a' + (d - 10u));
        v >>= 4;
    } while (v != 0 && n < 16);
    while (n > 0) put_ch(c, rev[--n]);
}

/* |v| as a magnitude, correct even for INT64_MIN (-(v+1) can't overflow). */
static uint64_t abs_i64(int64_t v) {
    return v < 0 ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
}

/* ----------------------------------------------------- fixed-point %f --
 * whole.fraction with `nd` fraction digits, round-half-away-from-zero,
 * carry rippling into `whole`. No <math.h>, and no static table — a
 * `static const` array would get RAM-shadowed on classic AVR, and 10^nd
 * for nd<=9 is a handful of multiplies. */
static void emit_fixed(fmtctx *c, double v, int nd) {
    if (nd < 0) nd = 0;
    if (nd > 9) nd = 9;
    if (v < 0.0) { put_ch(c, '-'); v = -v; }

    uint64_t scale = 1u;
    for (int k = 0; k < nd; k++) scale *= 10u;
    uint64_t whole = (uint64_t)v;
    double frac_d = (v - (double)whole) * (double)scale;
    uint64_t frac = (uint64_t)(frac_d + 0.5);
    if (frac >= scale) { frac -= scale; whole += 1u; }

    emit_u64_dec(c, whole);
    if (nd > 0) {
        put_ch(c, '.');
        char digits[9];
        for (int k = 0; k < nd; k++) digits[k] = '0';
        int p = nd - 1;
        while (frac > 0 && p >= 0) {
            digits[p--] = (char)('0' + (unsigned)(frac % 10u));
            frac /= 10u;
        }
        for (int k = 0; k < nd; k++) put_ch(c, digits[k]);
    }
}

/* ------------------------------------------------------------ driver --
 */
size_t janus_format_into(char *buf, size_t cap, const char *fmt,
                         const janus_bind_t *bind, const void *bound_struct) {
    fmtctx c = { buf, cap, 0 };
    bool have_arg = (bind != NULL && bind->field_type != JANUS_FIELD_NONE);
    bool consumed = false;

    if (cap == 0) return 0;

    size_t i = 0;
    for (;;) {
        char ch = fmt_at(fmt, i);
        if (ch == '\0') break;
        if (ch != '%') { put_ch(&c, ch); i++; continue; }

        /* --- at a '%': parse one spec, s..j inclusive --- */
        size_t s = i;
        size_t j = i + 1;
        char n = fmt_at(fmt, j);

        if (n == '%') { put_ch(&c, '%'); i = j + 1; continue; }
        if (n == '\0') { put_ch(&c, '%'); break; }

        int prec = -1;
        if (n == '.') {
            char pd = fmt_at(fmt, j + 1);
            if (pd >= '0' && pd <= '9') { prec = pd - '0'; j += 2; n = fmt_at(fmt, j); }
            /* else: leave n == '.', it fails the conv check below and the
             * whole spec is emitted verbatim */
        }

        int lng = 0;
        while (n == 'l' && lng < 2) { lng++; j++; n = fmt_at(fmt, j); }

        char conv = n;
        bool known = (conv == 'd' || conv == 'u' || conv == 'x' ||
                      conv == 's' || conv == 'f');

        if (!known || !have_arg || consumed) {
            emit_flash_span(&c, fmt, s, j);   /* stops at NUL on its own */
            if (conv == '\0') break;
            i = j + 1;
            continue;
        }

        consumed = true;
        if (conv == 's') {
            const char *sp = janus_read_bound_string(bind, bound_struct);
            put_ram(&c, sp != NULL ? sp : "(null)");
        } else if (conv == 'f') {
            emit_fixed(&c, janus_read_bound_value(bind, bound_struct),
                       prec < 0 ? 2 : prec);
        } else {
            int64_t iv = (int64_t)janus_read_bound_value(bind, bound_struct);
            if (conv == 'd') {
                int64_t v = lng >= 1 ? iv : (int64_t)(int32_t)iv;
                if (v < 0) put_ch(&c, '-');
                emit_u64_dec(&c, abs_i64(v));
            } else {
                uint64_t v = lng >= 1 ? (uint64_t)iv : (uint64_t)(uint32_t)iv;
                if (conv == 'u') emit_u64_dec(&c, v);
                else             emit_u64_hex(&c, v);
            }
        }
        i = j + 1;
    }

    c.buf[c.out] = '\0';
    return c.out;
}
