/* channel_icons task 4 — keep near-read flash data below AVR's 64 KiB
 * window. Forces the baked-image section (`.janus_img`, populated by the
 * generated `<id>_px` arrays via JANUS_IMG_SECTION — see janus_progmem.h)
 * to link *after* `.text`, so the font glyph tables, widget descriptors
 * and flash strings that the runtime reads with near `pgm_read_*` stay
 * under 0x10000 regardless of how much image data a project bakes.
 *
 * A supplement to the default linker script, not a replacement (`INSERT
 * AFTER`). Applied automatically for a CMake consumer by CMakeLists.txt's
 * `target_link_options(janus_runtime INTERFACE -Wl,-T,<this>)`; a
 * non-CMake AVR build can pass `-Wl,-T <path>/janus_img.ld` itself, or
 * just rely on the linker's orphan-section placement, which already
 * trails a read-only section after `.text`. Matches nothing on a non-AVR
 * host link (the arrays are ordinary `.rodata` there), so it is inert if
 * it is ever fed to one. */
SECTIONS
{
  .janus_img : { *(.janus_img*) }
} INSERT AFTER .text;
