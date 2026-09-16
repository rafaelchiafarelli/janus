import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_app, parse_screen
from janus.ir import App, DisplayConfig, NavTarget, Rect, Screen, Widget
from janus.stage2_layout.layout import (
    NAV_BAR_H,
    build_nav_bar,
    check_fits_display,
    layout_screen,
)

FIXTURES = Path(__file__).parent / "fixtures"


class TestLayoutUserProfile(unittest.TestCase):
    def setUp(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        self.screen = layout_screen(screen)

    def test_leaf_with_default_size(self) -> None:
        name_label = self.screen.root.children[0]
        self.assertEqual(name_label.geometry, Rect(x=0, y=0, w=110, h=32))

    def test_row_and_its_children(self) -> None:
        row = self.screen.root.children[1]
        caption, bar = row.children
        self.assertEqual(caption.geometry, Rect(x=0, y=36, w=110, h=32))
        self.assertEqual(bar.geometry, Rect(x=114, y=36, w=80, h=12))  # 110 + GAP(4)
        self.assertEqual(row.geometry, Rect(x=0, y=36, w=194, h=32))

    def test_root_size_derived_from_children(self) -> None:
        self.assertEqual(self.screen.root.geometry, Rect(x=0, y=0, w=194, h=68))


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


class TestLayoutBoxSummary(unittest.TestCase):
    def test_summary_widgets_right_aligned_in_header(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Summary",
            root=Widget(kind="column", id="root", children=[
                Widget(
                    kind="box", id="drawer", layout="column",
                    summary=[
                        Widget(kind="led", id="led", size=(10, 10)),
                        Widget(kind="label", id="mode", size=(20, 12)),
                    ],
                    children=[Widget(kind="label", id="detail")],
                ),
            ]),
        )
        layout_screen(screen)
        box = screen.root.children[0]
        led, mode = box.summary
        # box.w is derived from its (wider) detail child: label default 110px
        self.assertEqual(box.geometry.w, 110)
        # right-aligned: mode ends flush with the box's right edge, led sits
        # GAP(4) to its left
        self.assertEqual(mode.geometry, Rect(x=90, y=2, w=20, h=12))  # (110-20)=90, (16-12)/2=2
        self.assertEqual(led.geometry, Rect(x=76, y=3, w=10, h=10))  # 90-4-10=76, (16-10)/2=3

    def test_header_grows_to_fit_a_tall_summary_widget(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="TallSummary",
            root=Widget(kind="column", id="root", children=[
                Widget(
                    kind="box", id="drawer", layout="column",
                    summary=[Widget(kind="image", id="icon", size=(24, 24))],
                    children=[Widget(kind="label", id="detail")],
                ),
            ]),
        )
        layout_screen(screen)
        box = screen.root.children[0]
        self.assertEqual(box.geometry_collapsed.h, 24)  # taller than BOX_HEADER_H(16)
        # the detail child starts right below the grown header, not at 16
        self.assertEqual(box.children[0].geometry.y, 24)

    def test_box_widens_to_fit_a_summary_row_wider_than_its_children(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="WideSummary",
            root=Widget(kind="column", id="root", children=[
                Widget(
                    kind="box", id="drawer", layout="column",
                    summary=[
                        Widget(kind="led", id="led", size=(20, 12)),
                        Widget(kind="label", id="state", size=(50, 12)),
                        Widget(kind="label", id="freq", size=(50, 12)),
                    ],  # 20+50+50 + GAP(4)*2 = 128
                    children=[Widget(kind="label", id="detail", size=(30, 12))],  # narrower
                ),
            ]),
        )
        layout_screen(screen)
        box = screen.root.children[0]
        self.assertEqual(box.geometry.w, 128)  # widened past the 30px children would derive
        # right-aligned summary still starts on-screen, not negative
        self.assertGreaterEqual(box.summary[0].geometry.x, box.geometry.x)

    def test_collapsible_box_reserves_the_16px_header_strip(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Collapsible",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="box", id="drawer", layout="column", collapsible=True, children=[
                    Widget(kind="label", id="detail"),
                ]),
            ]),
        )
        layout_screen(screen)
        self.assertEqual(screen.root.children[0].geometry_collapsed.h, 16)

    def test_titled_box_reserves_the_16px_header_strip(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Titled",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="box", id="drawer", layout="column", text="Network", children=[
                    Widget(kind="label", id="detail"),
                ]),
            ]),
        )
        layout_screen(screen)
        self.assertEqual(screen.root.children[0].geometry_collapsed.h, 16)

    def test_bare_grouping_box_reserves_no_header_strip(self) -> None:
        # not collapsible, no title text, no summary -> a pure grouping
        # container; reserving/painting a 16px strip above its children
        # just ate content area (2026-09-07).
        from janus.ir import Screen, Widget

        screen = Screen(
            name="Bare",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="box", id="drawer", layout="column", children=[
                    Widget(kind="label", id="detail", size=(40, 12)),
                ]),
            ]),
        )
        layout_screen(screen)
        box = screen.root.children[0]
        self.assertEqual(box.geometry_collapsed.h, 0)
        # child sits at the box's own top edge, not pushed down by a strip
        self.assertEqual(box.children[0].geometry.y, box.geometry.y)
        self.assertEqual(box.geometry.h, 12)


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

        for kind, kwargs in (
            ("badge", {}), ("slider", {"range": (0, 100)}), ("vu", {"range": (0, 100)}),
        ):
            screen = Screen(
                name="Bad",
                root=Widget(kind="column", id="root", children=[
                    Widget(kind=kind, id="w", **kwargs),
                ]),
            )
            with self.assertRaises(ValueError):
                layout_screen(screen)

    def test_vu_geometry_matches_authored_size(self) -> None:
        from janus.ir import Screen, Widget

        screen = Screen(
            name="VuSized",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="vu", id="v", range=(0, 100), size=(80, 48)),
            ]),
        )
        layout_screen(screen)
        vu = screen.root.children[0]
        self.assertEqual((vu.geometry.w, vu.geometry.h), (80, 48))


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
                Widget(kind="header", id="h", text="Title"),  # default 140x32
                Widget(kind="label", id="body", fill=True),
            ]),
        )
        layout_screen(screen, DisplayConfig(width=200, height=100, color="mono"))
        header, body = screen.root.children
        self.assertEqual(header.geometry, Rect(x=0, y=0, w=140, h=32))
        # 100 - 32 (header) - GAP(4) = 64 left over for the fill label
        self.assertEqual(body.geometry, Rect(x=0, y=36, w=110, h=64))
        self.assertEqual(screen.root.geometry, Rect(x=0, y=0, w=140, h=100))

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
                Widget(kind="header", id="h", text="Title"),  # default 140x32
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
                        Widget(kind="label", id="right"),  # default 110x32
                    ],
                ),
            ]),
        )
        layout_screen(screen, DisplayConfig(width=200, height=90, color="mono"))
        middle = screen.root.children[0]
        left, right = middle.children
        self.assertEqual(middle.geometry, Rect(x=0, y=0, w=200, h=90))
        # middle's own width (200) is now known -> left fills leftover width:
        # 200 - 110 (right, default) - GAP(4) = 86
        self.assertEqual(left.geometry, Rect(x=0, y=0, w=86, h=32))
        self.assertEqual(right.geometry, Rect(x=90, y=0, w=110, h=32))


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


class TestLayoutNavBar(unittest.TestCase):
    def _screen(self) -> Screen:
        # a non-fill column root holding one fill child, so the child's
        # height tracks whatever available height the root is handed
        return Screen(
            name="S",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="column", id="body", fill=True, children=[
                    Widget(kind="label", id="l", text="hi", size=(100, 20)),
                ]),
            ]),
        )

    def test_no_nav_lays_out_unchanged(self) -> None:
        d = DisplayConfig(width=200, height=300, color="rgb565")
        s = layout_screen(self._screen(), d, has_nav=False)
        self.assertEqual(s.root.geometry.y, 0)
        self.assertEqual(s.root.children[0].geometry.h, 300)   # fill child gets the whole panel

    def test_nav_offsets_root_and_shrinks_fill_height(self) -> None:
        d = DisplayConfig(width=200, height=300, color="rgb565")
        s = layout_screen(self._screen(), d, has_nav=True)
        self.assertEqual(s.root.geometry.y, NAV_BAR_H)                       # pushed below the band
        self.assertEqual(s.root.children[0].geometry.h, 300 - NAV_BAR_H)    # fill child lost the band
        self.assertEqual(s.root.geometry.y + s.root.geometry.h, 300)        # still bottoms out at the panel

    def test_check_fits_display_counts_the_band(self) -> None:
        # a screen that's exactly panel-height at y=0 no longer fits once
        # the nav band shifts it down by NAV_BAR_H
        s = Screen(name="S", root=Widget(kind="column", id="root", children=[
            Widget(kind="label", id="l", text="x", size=(50, 300)),
        ]))
        d = DisplayConfig(width=200, height=300, color="rgb565")
        layout_screen(s, d, has_nav=True)
        with self.assertRaises(ValueError):
            check_fits_display(s, d)

    def _nav_app(self) -> App:
        screens = [Screen(name=n, root=Widget(kind="column", id="r", children=[]))
                   for n in ("Alpha", "Bravo", "Charlie")]
        return App(
            screens=screens,
            nav=[NavTarget(screen="Bravo", title="B"),
                 NavTarget(screen="Alpha", title="A"),
                 NavTarget(screen="Charlie", title="C")],
            display=DisplayConfig(width=320, height=480, color="rgb565"),
        )

    def test_build_nav_bar_equal_cells_last_absorbs_remainder(self) -> None:
        tabs = build_nav_bar(self._nav_app())
        self.assertEqual([t.rect for t in tabs], [
            Rect(x=0, y=0, w=106, h=NAV_BAR_H),
            Rect(x=106, y=0, w=106, h=NAV_BAR_H),
            Rect(x=212, y=0, w=108, h=NAV_BAR_H),   # 320 % 3 == 2 -> last cell
        ])

    def test_build_nav_bar_target_index_follows_nav_order_not_screen_order(self) -> None:
        tabs = build_nav_bar(self._nav_app())
        self.assertEqual([t.title for t in tabs], ["B", "A", "C"])
        self.assertEqual([t.target_screen_index for t in tabs], [1, 0, 2])

    def test_build_nav_bar_none_without_nav(self) -> None:
        app = self._nav_app()
        app.nav = None
        self.assertIsNone(build_nav_bar(app))


if __name__ == "__main__":
    unittest.main()
