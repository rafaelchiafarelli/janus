import tempfile
import unittest
from pathlib import Path

from PIL import Image

from janus.stage3b_embedded_c.image_asset import (
    ImageAssetError,
    load_rgb565,
    pack_rgb565,
)


class TestPackRgb565(unittest.TestCase):
    def test_pure_channels(self) -> None:
        self.assertEqual(pack_rgb565(0, 0, 0), 0x0000)
        self.assertEqual(pack_rgb565(255, 255, 255), 0xFFFF)
        self.assertEqual(pack_rgb565(255, 0, 0), 0xF800)
        self.assertEqual(pack_rgb565(0, 255, 0), 0x07E0)
        self.assertEqual(pack_rgb565(0, 0, 255), 0x001F)


class TestLoadRgb565(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)

    def _save(self, name: str, img: Image.Image) -> str:
        path = self.dir / name
        img.save(path)
        return str(path)

    def test_decodes_and_packs_a_solid_png(self) -> None:
        path = self._save("red.png", Image.new("RGB", (8, 8), (255, 0, 0)))
        px = load_rgb565(path, 8, 8)
        self.assertEqual(len(px), 64)
        self.assertTrue(all(p == 0xF800 for p in px))

    def test_rescales_to_the_requested_size(self) -> None:
        path = self._save("big.png", Image.new("RGB", (64, 40), (0, 0, 255)))
        px = load_rgb565(path, 10, 6)
        self.assertEqual(len(px), 60)
        self.assertTrue(all(p == 0x001F for p in px))

    def test_row_major_order_row_zero_first(self) -> None:
        # top half red, bottom half green — first pixel must be red.
        src = Image.new("RGB", (4, 4), (255, 0, 0))
        for y in range(2, 4):
            for x in range(4):
                src.putpixel((x, y), (0, 255, 0))
        path = self._save("split.png", src)
        px = load_rgb565(path, 4, 4)
        self.assertEqual(px[0], 0xF800)
        self.assertEqual(px[-1], 0x07E0)

    def test_alpha_is_composited_over_black(self) -> None:
        # fully transparent white -> should come out black, not white.
        path = self._save("clear.png", Image.new("RGBA", (4, 4), (255, 255, 255, 0)))
        px = load_rgb565(path, 4, 4)
        self.assertTrue(all(p == 0x0000 for p in px))

    def test_partial_alpha_darkens_toward_black(self) -> None:
        path = self._save("half.png", Image.new("RGBA", (2, 2), (255, 255, 255, 128)))
        px = load_rgb565(path, 2, 2)
        # ~50% over black -> mid grey, definitely neither white nor black.
        self.assertTrue(all(0 < p < 0xFFFF for p in px))

    def test_missing_file_raises_image_asset_error(self) -> None:
        with self.assertRaises(ImageAssetError):
            load_rgb565(str(self.dir / "nope.png"), 8, 8)

    def test_undecodable_file_raises_image_asset_error(self) -> None:
        junk = self.dir / "junk.png"
        junk.write_bytes(b"this is not an image")
        with self.assertRaises(ImageAssetError):
            load_rgb565(str(junk), 8, 8)

    def test_degenerate_target_size_raises(self) -> None:
        path = self._save("ok.png", Image.new("RGB", (8, 8), (1, 2, 3)))
        with self.assertRaises(ImageAssetError):
            load_rgb565(path, 0, 8)

    def test_bmp_is_supported(self) -> None:
        path = self._save("solid.bmp", Image.new("RGB", (8, 8), (0, 255, 0)))
        px = load_rgb565(path, 8, 8)
        self.assertTrue(all(p == 0x07E0 for p in px))


if __name__ == "__main__":
    unittest.main()
