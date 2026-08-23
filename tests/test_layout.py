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


class TestLayoutFill(unittest.TestCase):
    def test_single_fill_child_consumes_leftover_height(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Fill",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="header", id="h", text="Title"),  # default 80x16
                Widget(kind="label", id="body", fill=True),
            ]),
        )
        layout_screen(screen, DisplayConfig(width=200, height=100, color="mono"))
        header, body = screen.root.children
        self.assertEqual(header.geometry, Rect(x=0, y=0, w=80, h=16))
        # 100 - 16 (header) - GAP(4) = 80 left over for the fill label
        self.assertEqual(body.geometry, Rect(x=0, y=20, w=60, h=80))
        self.assertEqual(screen.root.geometry, Rect(x=0, y=0, w=80, h=100))

    def test_two_fill_siblings_split_evenly_remainder_on_last(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Fill",
            root=Widget(kind="row", id="root", children=[
                Widget(kind="label", id="a", fill=True),
                Widget(kind="label", id="b", fill=True),
            ]),
        )
        layout_screen(screen, DisplayConfig(width=101, height=50, color="mono"))
        a, b = screen.root.children
        # 101 - GAP(4) = 97 leftover; 97 // 2 = 48, remainder 1 goes to b
        self.assertEqual(a.geometry.w, 48)
        self.assertEqual(b.geometry.w, 49)

    def test_fill_without_a_known_size_raises(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Fill",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="label", id="body", fill=True),
            ]),
        )
        with self.assertRaises(ValueError):
            layout_screen(screen)  # no display -> root's own height is unknown

    def test_fill_children_overflowing_available_space_raises(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Fill",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="header", id="h", text="Title"),  # default 80x16
                Widget(kind="label", id="body", fill=True),
            ]),
        )
        with self.assertRaises(ValueError):
            layout_screen(screen, DisplayConfig(width=200, height=10, color="mono"))

    def test_fill_propagates_into_a_nested_container(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Fill",
            root=Widget(kind="column", id="root", children=[
                Widget(
                    kind="row", id="middle", fill=True, children=[
                        Widget(kind="label", id="left", fill=True),
                        Widget(kind="label", id="right"),  # default 60x12
                    ],
                ),
            ]),
        )
        layout_screen(screen, DisplayConfig(width=200, height=90, color="mono"))
        middle = screen.root.children[0]
        left, right = middle.children
        self.assertEqual(middle.geometry, Rect(x=0, y=0, w=200, h=90))
        # middle's own width (200) is now known -> left fills leftover width:
        # 200 - 60 (right, default) - GAP(4) = 136
        self.assertEqual(left.geometry, Rect(x=0, y=0, w=136, h=12))
        self.assertEqual(right.geometry, Rect(x=140, y=0, w=60, h=12))


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
