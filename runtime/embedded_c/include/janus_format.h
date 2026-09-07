/* Janus embedded-C runtime — label/header format templates.
 *
 * A `label`/`header` whose authored `text:` holds an unescaped `%`
 * conversion and which also has a `bind:` renders the live bound value
 * interpolated into that template — `text: "Duty: %d%%"` over an int
 * bind of 72 draws `Duty: 72%`. This is the allocation-free formatter
 * that does the substitution; the Stage 1 recognition + `draw_label` /
 * `draw_header` wiring is label_format task 2.
 *
 * Deliberately NOT libc: no `snprintf`, no `<math.h>`. One conversion is
 * consumed (a widget carries exactly one `bind:`); anything else in the
 * string — a second conversion, an unknown letter, every conversion when
 * the widget is unbound — is copied through verbatim.
 */
#ifndef JANUS_FORMAT_H
#define JANUS_FORMAT_H

#include <stddef.h>

#include "janus_runtime.h"   /* janus_bind_t */

/* Stack buffer draw_label / draw_header format a template into, per draw.
 * A rendered label longer than this is truncated here (and then clipped
 * again by draw_string's own right-edge clip on any realistic widget
 * width anyway) — 47 visible chars is well past what fits a 240/320-class
 * panel row at either font size. */
#define JANUS_FORMAT_BUF 48

/* Formats `fmt` into `buf`, substituting the one bound value/string from
 * `bind` + `bound_struct`. Writes at most `cap - 1` characters plus a
 * terminating NUL and never past `cap`; returns the number of characters
 * actually written (strlen(buf)), which is clamped by `cap` — it is NOT
 * the length the result would have had with unlimited space.
 *
 * `fmt` is flash-resident on AVR (read one byte at a time via
 * JANUS_PGM_READ_U8, same as draw_string's from_flash path). `bind` may
 * be NULL / JANUS_FIELD_NONE — then every conversion is emitted literally.
 *
 * Supported conversions (exactly one is filled, first one wins):
 *   %d   signed decimal   — bound value via (int32_t), or (int64_t) with an l/ll length
 *   %u   unsigned decimal — (uint32_t), or (uint64_t) with l/ll
 *   %x   lowercase hex, no 0x — same widths as %u
 *   %ld %lld  signed 64-bit decimal
 *   %f   fixed-point, 2 fraction digits by default
 *   %.Nf fixed-point, N fraction digits (N in 0..9)
 *   %s   the bound string; a NULL bound string prints "(null)"
 *   %%   a literal percent
 */
size_t janus_format_into(char *buf, size_t cap, const char *fmt,
                         const janus_bind_t *bind, const void *bound_struct);

#endif /* JANUS_FORMAT_H */
