"""Stage 3b — image asset decoding.

Turns a widget's `file:` image into a flat list of RGB565 values,
rescaled to the widget's authored size, ready to bake into the generated
screen source as a `JANUS_PROGMEM` array.

Kept separate from `emit_embedded_c` so the Pillow dependency sits behind
one import and the decode / rescale / alpha / pack logic stays unit
testable on its own. Every failure here is an `ImageAssetError` the
caller is expected to log and fall back from (a magenta placeholder) —
never a hard generation abort. That "log, don't stop" contract is the
whole reason this returns-or-raises instead of writing anything itself.
"""
from __future__ import annotations

import logging
from pathlib import Path

log = logging.getLogger("janus.image")

# Past this many pixels a single image is a real flash cost (2 bytes each,
# so ~16 KiB at 8192). Not an error — the widget's `size` is an explicit
# authored choice — just worth a line in the build log.
_WARN_PIXELS = 8192

# avr-gcc rejects a single object of >= 32768 bytes ("size of variable is
# too large"), independent of near/far placement — so a baked RGB565
# array can't exceed 16383 pixels (~128x128 square) on a classic-AVR
# target. Still just a warning here: another target (or a future tiled
# multi-array bake) may not have the limit, and generation never aborts
# on an image.
_AVR_MAX_PIXELS = 16383


class ImageAssetError(Exception):
    """Lookup / format / decode failure. The caller logs it and renders
    the widget as a magenta placeholder; generation continues."""


def pack_rgb565(r: int, g: int, b: int) -> int:
    """(r, g, b) 8-bit each -> packed 16-bit RGB565 (5 R, 6 G, 5 B).
    Same packing as emit_embedded_c._pack_rgb565, just from a tuple
    instead of a "#RRGGBB" string."""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def load_rgb565(path: str, width: int, height: int) -> list[int]:
    """Decode the image at `path`, composite any alpha over opaque black,
    rescale to `width` x `height` (LANCZOS), and return `width * height`
    RGB565 values row-major, row 0 first — matching the `draw_area_sync`
    driver contract the runtime blits them through.

    Raises `ImageAssetError` on a missing file, an unsupported / corrupt
    format, or a degenerate target size.
    """
    if width <= 0 or height <= 0:
        raise ImageAssetError(f"target size {width}x{height} is degenerate")

    file_path = Path(path)
    if not file_path.is_file():
        raise ImageAssetError(f"file not found: {path}")

    try:
        from PIL import Image, UnidentifiedImageError
    except ImportError as exc:  # pragma: no cover - Pillow is a hard dep
        raise ImageAssetError(f"Pillow not available: {exc}") from exc

    try:
        with Image.open(file_path) as source:
            source.load()
            has_alpha = source.mode in ("RGBA", "LA", "PA") or "transparency" in source.info
            if has_alpha:
                rgba = source.convert("RGBA")
                black = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
                black.alpha_composite(rgba)
                rgb = black.convert("RGB")
            else:
                rgb = source.convert("RGB")
            if rgb.size != (width, height):
                rgb = rgb.resize((width, height), Image.Resampling.LANCZOS)
            raw = rgb.tobytes()   # tightly packed R,G,B bytes, row-major, row 0 first
    except (UnidentifiedImageError, OSError, ValueError) as exc:
        raise ImageAssetError(f"cannot decode {path}: {exc}") from exc

    count = width * height
    if len(raw) != count * 3:  # pragma: no cover - guards against a mode we didn't expect
        raise ImageAssetError(
            f"{path}: decoded {len(raw)} bytes for a {width}x{height} RGB image, expected {count * 3}"
        )
    if count > _AVR_MAX_PIXELS:
        log.warning(
            "%s scaled to %dx%d = %d px (%d bytes): a single baked array this big "
            "won't compile for a classic-AVR target (avr-gcc caps one object at "
            "32768 bytes) — cap the widget `size` near 128x128",
            path, width, height, count, count * 2,
        )
    elif count > _WARN_PIXELS:
        log.warning(
            "%s scaled to %dx%d = %d px (~%d KiB of flash); a smaller `size` would cost less",
            path, width, height, count, count * 2 // 1024,
        )
    return [pack_rgb565(raw[i], raw[i + 1], raw[i + 2]) for i in range(0, len(raw), 3)]
