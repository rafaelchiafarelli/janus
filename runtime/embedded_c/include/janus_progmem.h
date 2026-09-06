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

#include <stdint.h>
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

/* ------------------------------------------------------- far flash reads --
 * Big generated data (baked RGB565 image arrays) can link past the 64 KiB
 * that an ordinary 16-bit flash pointer / `memcpy_P` can reach on a
 * classic AVR (ATmega2560 has 256 KiB). `janus_farptr_t` addresses the
 * whole flash; `JANUS_MEMCPY_PF` copies from it.
 *
 * Key constraint: a far address is *not* a link-time constant on AVR
 * (`pgm_get_far_address` expands to inline asm), so `JANUS_FAR_ADDR` can
 * only be used inside a function body — never a `static const`
 * initializer. That's why generated screen code fills a small RAM table
 * of these from a resolver function the runtime calls once per
 * screen-enter, instead of storing pointers in the flash descriptors.
 * Off-AVR everything degrades to plain pointers, so host builds/tests are
 * unchanged. */
#if defined(__AVR__)
typedef uint_farptr_t janus_farptr_t;
#define JANUS_FAR_ADDR(sym)            pgm_get_far_address(sym)
#define JANUS_FAR_ADD(base, byte_off)  ((janus_farptr_t)((base) + (byte_off)))
#define JANUS_MEMCPY_PF(dst, far, n)   memcpy_PF((dst), (far), (n))
#else
typedef const uint16_t *janus_farptr_t;
#define JANUS_FAR_ADDR(sym)            (sym)
#define JANUS_FAR_ADD(base, byte_off)  ((janus_farptr_t)((const uint8_t *)(base) + (byte_off)))
#define JANUS_MEMCPY_PF(dst, far, n)   memcpy((dst), (const void *)(far), (n))
#endif

/* --------------------------------------------------- baked-image storage --
 * The generated RGB565 image arrays (`<id>_px`) use this, *not*
 * JANUS_PROGMEM. On AVR they go into their own `.janus_img` flash
 * section so the linker places the whole lot after `.text` — which keeps
 * the font glyph tables, widget descriptors and flash strings (all read
 * with *near* `pgm_read_*` / `memcpy_P`) below the 64 KiB near-flash
 * window (channel_icons task 4). `runtime/embedded_c/janus_img.ld`
 * pins that placement (`INSERT AFTER .text`); with no linker script the
 * linker's orphan-section rule already trails a read-only section after
 * `.text`, so a bare `avr-gcc` link works too.
 *
 * Deliberately not `__progmem__`: avr-gcc silently drops a `section`
 * attribute when `__progmem__` is also present. The arrays are still
 * flash-only — a `const` in a read-only section the linker maps into the
 * text region, no RAM shadow — and every read of them goes through
 * `JANUS_FAR_ADDR` + `JANUS_MEMCPY_PF` (the descriptor carries a slot
 * index, never a pointer, so nothing ever near-derefs one). `used` keeps
 * `-fdata-sections` + `--gc-sections` from dropping an array whose only
 * reference is the generated `resolve_images()`. Off-AVR it is just
 * JANUS_PROGMEM (i.e. nothing), so host builds/tests are unchanged. */
#if defined(__AVR__)
#define JANUS_IMG_SECTION __attribute__((used, section(".janus_img")))
#else
#define JANUS_IMG_SECTION JANUS_PROGMEM
#endif

#endif /* JANUS_PROGMEM_H */
