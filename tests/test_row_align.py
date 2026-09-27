"""`align` on a row: cross-axis (vertical) placement of its children,
resolved by Stage 2 (fixes/000008)."""
import unittest

from janus.ir import DisplayConfig
from janus.stage1_parse.dsl_yaml import screen_from_dict
from janus.stage2_layout.layout import FOCUS_RING_PAD, layout_screen


def _screen(children):
    return {"screen": "Align", "layout": "column", "children": children}


def _relay_row(**extra):
    # toggle 24x12 (default), label 110x32 (default), led 16x16
    return {"kind": "row", "id": "r", **extra, "children": [
        {"kind": "toggle", "id": "t", "on_press": "go"},
        {"kind": "label", "id": "l", "text": "Relay 0"},
        {"kind": "led", "id": "d", "size": {"w": 16, "h": 16}},
    ]}


def _layout(children, display=None):
    return layout_screen(screen_from_dict(_screen(children)), display)


class TestRowAlignLayout(unittest.TestCase):
    def _ys(self, row):
        return [c.geometry.y - row.geometry.y for c in row.children]

    def test_default_is_top(self) -> None:
        row = _layout([_relay_row()]).root.children[0]
        self.assertEqual(self._ys(row), [0, 0, 0])

    def test_explicit_top_is_identical_to_default(self) -> None:
        a = _layout([_relay_row()]).root.children[0]
        b = _layout([_relay_row(align="top")]).root.children[0]
        self.assertEqual([c.geometry for c in a.children], [c.geometry for c in b.children])

    def test_center(self) -> None:
        row = _layout([_relay_row(align="center")]).root.children[0]
        self.assertEqual(row.geometry.h, 32)
        self.assertEqual(self._ys(row), [10, 0, 8])   # (32-12)/2, tallest, (32-16)/2

    def test_bottom(self) -> None:
        row = _layout([_relay_row(align="bottom")]).root.children[0]
        self.assertEqual(self._ys(row), [20, 0, 16])

    def test_center_inside_a_focus_ring_row(self) -> None:
        p = FOCUS_RING_PAD
        row = _layout([_relay_row(align="center", focus_ring=True)]).root.children[0]
        self.assertEqual(row.geometry.h, 32 + 2 * p)
        self.assertEqual(self._ys(row), [p + 10, p, p + 8])   # centred inside the ring inset

    def test_x_is_untouched(self) -> None:
        a = _layout([_relay_row()]).root.children[0]
        b = _layout([_relay_row(align="center")]).root.children[0]
        self.assertEqual([(c.geometry.x, c.geometry.w, c.geometry.h) for c in a.children],
                         [(c.geometry.x, c.geometry.w, c.geometry.h) for c in b.children])

    def test_plain_row_centres_a_ring_row_and_its_contents(self) -> None:
        # The PWM "A" row: [ring row(toggle 28x18) -> 40x30] [label "A" 18x18].
        row = _layout([{"kind": "row", "id": "out", "align": "center", "children": [
            {"kind": "row", "id": "ring", "focus_ring": True, "children": [
                {"kind": "toggle", "id": "t", "on_press": "go", "size": {"w": 28, "h": 18}}]},
            {"kind": "label", "id": "a", "text": "A", "size": {"w": 18, "h": 18}},
        ]}]).root.children[0]
        ring, label = row.children
        toggle = ring.children[0]
        self.assertEqual(ring.geometry.h, 30)
        self.assertEqual(label.geometry.y - row.geometry.y, 6)       # (30-18)/2
        self.assertEqual(label.geometry.y, toggle.geometry.y)        # letter level with its switch

    def test_nested_subtree_moves_with_its_parent(self) -> None:
        row = _layout([{"kind": "row", "id": "out", "align": "bottom", "children": [
            {"kind": "label", "id": "tall", "text": "x", "size": {"w": 20, "h": 40}},
            {"kind": "column", "id": "col", "children": [
                {"kind": "label", "id": "c1", "text": "a", "size": {"w": 20, "h": 10}},
                {"kind": "label", "id": "c2", "text": "b", "size": {"w": 20, "h": 10}}]},
        ]}]).root.children[0]
        col = row.children[1]
        self.assertEqual(col.geometry.y - row.geometry.y, 40 - 24)
        self.assertEqual(col.children[0].geometry.y, col.geometry.y)
        self.assertEqual(col.children[1].geometry.y, col.geometry.y + 14)

    def test_box_with_layout_row_centres_below_its_header(self) -> None:
        box = _layout([{"kind": "box", "id": "b", "layout": "row", "text": "Bus",
                        "align": "center", "children": [
            {"kind": "led", "id": "d", "size": {"w": 10, "h": 10}},
            {"kind": "label", "id": "l", "text": "CAN0", "size": {"w": 50, "h": 18}}]}]).root.children[0]
        header_h = box.geometry_collapsed.h
        led, label = box.children
        self.assertEqual(label.geometry.y, box.geometry.y + header_h)
        self.assertEqual(led.geometry.y, box.geometry.y + header_h + 4)   # (18-10)/2

    def test_fill_row_centres_in_its_forced_height(self) -> None:
        screen = _layout([{"kind": "row", "id": "r", "fill": True, "align": "center", "children": [
            {"kind": "label", "id": "l", "text": "x", "size": {"w": 20, "h": 20}}]}],
            DisplayConfig(width=100, height=100, color="rgb565"))
        row = screen.root.children[0]
        self.assertEqual(row.geometry.h, 100)
        self.assertEqual(row.children[0].geometry.y - row.geometry.y, 40)


class TestRowAlignValidation(unittest.TestCase):
    def test_invalid_value(self) -> None:
        with self.assertRaisesRegex(ValueError, "invalid align"):
            screen_from_dict(_screen([_relay_row(align="middle")]))

    def test_column_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "only a `row`"):
            screen_from_dict(_screen([{"kind": "column", "id": "c", "align": "center",
                                       "children": [{"kind": "label", "id": "l", "text": "x"}]}]))

    def test_leaf_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "only a `row`"):
            screen_from_dict(_screen([{"kind": "label", "id": "l", "text": "x", "align": "center"}]))

    def test_row_with_layout_column_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "only a `row`"):
            screen_from_dict(_screen([{"kind": "row", "id": "r", "layout": "column", "align": "center",
                                       "children": [{"kind": "label", "id": "l", "text": "x"}]}]))


if __name__ == "__main__":
    unittest.main()
