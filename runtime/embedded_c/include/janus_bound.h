/* Janus embedded-C runtime — bound-field reads.
 *
 * The one owner of "resolve a janus_bind_t against a bound_struct
 * instance": the offsetof-into-the-generated-struct math lives here and
 * nowhere else. Two consumers today — janus_runtime.c's per-kind draw
 * functions (a live numeric value or string for progress/led/label/...)
 * and janus_format.c's label format templates (`text: "Duty: %d%%"`).
 * Split out of janus_runtime.c (where both were file-static) so the
 * formatter can share them without pulling the whole render module into
 * its link (label_format task 1).
 */
#ifndef JANUS_BOUND_H
#define JANUS_BOUND_H

#include "janus_runtime.h"   /* janus_bind_t, janus_field_type_t */

/* Reads the bound field as a double. 0.0 when `bound_struct` is NULL, the
 * binding is JANUS_FIELD_NONE, or the field is a string (no numeric
 * value). INT / INT64 / FLOAT are widened from their stored width. */
double janus_read_bound_value(const janus_bind_t *bind, const void *bound_struct);

/* Reads the bound field as a string pointer — the generated struct field
 * *is* a `const char *` (emit_bindings_struct.py), so this returns the
 * stored pointer itself, not a reinterpretation of bytes. NULL when
 * `bound_struct` is NULL, the field isn't JANUS_FIELD_STRING, or the
 * pointer was never populated (zero-initialised instance). */
const char *janus_read_bound_string(const janus_bind_t *bind, const void *bound_struct);

#endif /* JANUS_BOUND_H */
