import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.ir import DisplayConfig, Rect
from janus.stage2_layout.layout import check_fits_display, layout_screen

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

    def test_badge_and_slider_require_explicit_size(self) -> None:
        from janus.ir import Screen, Widget

        for kind, kwargs in (("badge", {}), ("slider", {"range": (0, 100)})):
            screen = Screen(
                name="Bad",
                root=Widget(kind="column", id="root", children=[
                    Widget(kind=kind, id="w", **kwargs),
                ]),
            )
            with self.assertRaises(ValueError):
                layout_screen(screen)


class TestLayoutNewLowEffortKinds(unittest.TestCase):
    def test_divider_gets_a_default_size(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Divider",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="divider", id="d"),
            ]),
        )
        layout_screen(screen)
        self.assertEqual(screen.root.children[0].geometry, Rect(x=0, y=0, w=60, h=2))

    def test_toggle_gets_a_default_size(self) -> None:
        from janus.ir import Binding, Screen, Widget

        screen = Screen(
            name="Toggle",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="toggle", id="t", bind=Binding(message="m", field="f", type="int")),
            ]),
        )
        layout_screen(screen)
        self.assertEqual(screen.root.children[0].geometry, Rect(x=0, y=0, w=24, h=12))


class TestCheckFitsDisplay(unittest.TestCase):
    def setUp(self) -> None:
        self.screen = layout_screen(parse_screen(FIXTURES / "user_profile.screen.yaml"))

    def test_screen_within_display_bounds_accepted(self) -> None:
        display = DisplayConfig(width=240, height=320, color="mono")
        check_fits_display(self.screen, display)  # must not raise

    def test_screen_too_wide_for_display_rejected(self) -> None:
        display = DisplayConfig(width=100, height=320, color="mono")
        with self.assertRaises(ValueError):
            check_fits_display(self.screen, display)

    def test_screen_too_tall_for_display_rejected(self) -> None:
        display = DisplayConfig(width=240, height=20, color="mono")
        with self.assertRaises(ValueError):
            check_fits_display(self.screen, display)


if __name__ == "__main__":
    unittest.main()
