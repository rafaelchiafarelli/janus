/* Janus embedded-C runtime — bound-field reads. See janus_bound.h.
 *
 * Moved verbatim out of janus_runtime.c (where they were `static
 * read_bound_value` / `read_bound_string`) so janus_format.c can share
 * the exact same offset math without dragging the render module's driver
 * symbols into a formatter-only test link (label_format task 1).
 */
#include "janus_bound.h"

#include <stddef.h>
#include <string.h>

double janus_read_bound_value(const janus_bind_t *bind, const void *bound_struct) {
    if (bound_struct == NULL || bind->field_type == JANUS_FIELD_NONE) return 0.0;
    const uint8_t *field = (const uint8_t *)bound_struct + bind->field_offset;
    switch (bind->field_type) {
        case JANUS_FIELD_INT: {
            int v;
            memcpy(&v, field, sizeof(v));
            return (double)v;
        }
        case JANUS_FIELD_INT64: {
            int64_t v;
            memcpy(&v, field, sizeof(v));
            return (double)v;
        }
        case JANUS_FIELD_FLOAT: {
            float v;
            memcpy(&v, field, sizeof(v));
            return (double)v;
        }
        default:
            return 0.0; /* string has no numeric value — see janus_read_bound_string */
    }
}

const char *janus_read_bound_string(const janus_bind_t *bind, const void *bound_struct) {
    if (bound_struct == NULL || bind->field_type != JANUS_FIELD_STRING) return NULL;
    const uint8_t *field = (const uint8_t *)bound_struct + bind->field_offset;
    const char *value;
    memcpy(&value, field, sizeof(value));
    return value;
}
