import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.ir import Rect
from janus.stage2_layout.layout import layout_screen

FIXTURES = Path(__file__).parent / "fixtures"


class TestLayoutUserProfile(unittest.TestCase):
    def setUp(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        self.screen = layout_screen(screen)

    def test_leaf_with_default_size(self) -> None:
        name_label = self.screen.root.children[0]
        self.assertEqual(name_label.geometry, Rect(x=0, y=0, w=60, h=12))

    def test_row_and_its_children(self) -> None:
        row = self.screen.root.children[1]
        caption, bar = row.children
        self.assertEqual(caption.geometry, Rect(x=0, y=16, w=60, h=12))
        self.assertEqual(bar.geometry, Rect(x=64, y=16, w=80, h=12))  # 60 + GAP(4)
        self.assertEqual(row.geometry, Rect(x=0, y=16, w=144, h=12))

    def test_root_size_derived_from_children(self) -> None:
        self.assertEqual(self.screen.root.geometry, Rect(x=0, y=0, w=144, h=28))


class TestLayoutBox(unittest.TestCase):
    def setUp(self) -> None:
        screen = parse_screen(FIXTURES / "box_demo.screen.yaml")
        self.screen = layout_screen(screen)
        self.box = self.screen.root.children[0]

    def test_box_expanded_covers_header_and_body(self) -> None:
        self.assertEqual(self.box.geometry, Rect(x=0, y=0, w=10, h=26))  # 16 header + 10 body

    def test_box_collapsed_covers_header_only(self) -> None:
        self.assertEqual(self.box.geometry_collapsed, Rect(x=0, y=0, w=10, h=16))

    def test_child_offset_below_header(self) -> None:
        led = self.box.children[0]
        self.assertEqual(led.geometry, Rect(x=0, y=16, w=10, h=10))


class TestLayoutSizeEnforcement(unittest.TestCase):
    def test_progress_without_size_raises(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Bad",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="progress", id="p", range=(0, 100)),
            ]),
        )
        with self.assertRaises(ValueError):
            layout_screen(screen)


if __name__ == "__main__":
    unittest.main()
