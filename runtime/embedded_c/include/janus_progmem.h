/* Janus embedded-C runtime — flash-storage portability shim.
 *
 * Generation-time-fixed UI data (widget/screen descriptor arrays, the
 * app-level screen/title tables, the font glyph table, and the string
 * literals inside all of those) is emitted `JANUS_PROGMEM` so classic AVR
 * targets (Harvard architecture — ordinary pointers only address SRAM)
 * keep it in flash only, instead of avr-gcc's default of also shadowing
 * every `const` global into RAM at startup. Off-AVR (host builds, tests),
 * every macro here degrades to the plain-C operation it replaces, so
 * behavior is unchanged there — this is what makes the flash-side of the
 * change verifiable without an AVR toolchain.
 */
#ifndef JANUS_PROGMEM_H
#define JANUS_PROGMEM_H

#include <string.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define JANUS_PROGMEM PROGMEM
#define JANUS_PGM_READ_U8(addr)  pgm_read_byte(addr)
#define JANUS_PGM_READ_PTR(addr) pgm_read_ptr(addr)
#define JANUS_MEMCPY_P(dst, src, n) memcpy_P((dst), (src), (n))
#else
#define JANUS_PROGMEM
#define JANUS_PGM_READ_U8(addr)  (*(const uint8_t *)(addr))
#define JANUS_PGM_READ_PTR(addr) (*(const void *const *)(addr))
#define JANUS_MEMCPY_P(dst, src, n) memcpy((dst), (src), (n))
#endif

#endif /* JANUS_PROGMEM_H */
