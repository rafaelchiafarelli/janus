"""Task: embedded_rendering / channel_icons / 1-hidden-widget-flag.

`hidden: true` is a Stage-1-only prune: the widget and its whole subtree
are dropped by the parser, so no later stage (layout, harpia emit,
embedded-C emit) ever sees it.
"""
import unittest

from janus.ir import App
from janus.stage1_parse.dsl_yaml import screen_from_dict
from janus.stage2_layout.layout import layout_screen
from janus.stage3a_harpia.emit_harpia import emit_harpia
from janus.stage3b_embedded_c.emit_embedded_c import emit_screen


def _screen(children):
    return {"screen": "Hid", "layout": "column", "children": children}


def _ids(widget):
    out = [widget.id]
    for c in widget.children:
        out += _ids(c)
    for c in widget.summary:
        out += _ids(c)
    return out


class TestHiddenPrune(unittest.TestCase):
    def test_hidden_leaf_dropped_from_children(self) -> None:
        screen = screen_from_dict(_screen([
            {"kind": "label", "id": "keep", "text": "a"},
            {"kind": "label", "id": "gone", "text": "b", "hidden": True},
        ]))
        self.assertEqual([c.id for c in screen.root.children], ["keep"])

    def test_hidden_container_takes_its_whole_subtree(self) -> None:
        screen = screen_from_dict(_screen([
            {"kind": "row", "id": "gone", "hidden": True, "children": [
                {"kind": "label", "id": "grandchild", "text": "x"},
            ]},
            {"kind": "label", "id": "keep", "text": "a"},
        ]))
        self.assertNotIn("gone", _ids(screen.root))
        self.assertNotIn("grandchild", _ids(screen.root))
        self.assertIn("keep", _ids(screen.root))

    def test_hidden_dropped_from_summary(self) -> None:
        screen = screen_from_dict(_screen([
            {"kind": "box", "id": "b", "layout": "column", "summary": [
                {"kind": "led", "id": "s_keep", "size": {"w": 8, "h": 8}},
                {"kind": "led", "id": "s_gone", "size": {"w": 8, "h": 8}, "hidden": True},
            ], "children": [
                {"kind": "label", "id": "body", "text": "x"},
            ]},
        ]))
        box = screen.root.children[0]
        self.assertEqual([w.id for w in box.summary], ["s_keep"])

    def test_hidden_top_level_child_dropped(self) -> None:
        screen = screen_from_dict(_screen([
            {"kind": "label", "id": "gone", "text": "b", "hidden": True},
            {"kind": "label", "id": "keep", "text": "a"},
        ]))
        self.assertEqual([c.id for c in screen.root.children], ["keep"])

    def test_non_bool_hidden_rejected(self) -> None:
        for bad in (1, "yes", "true", {}):
            with self.assertRaises(ValueError):
                screen_from_dict(_screen([
                    {"kind": "label", "id": "a", "text": "x", "hidden": bad},
                ]))

    def test_whole_screen_hidden_rejected(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "x"}])
        data["hidden"] = True
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_survivor_takes_the_hidden_slot_geometry(self) -> None:
        """A visible sibling after a hidden one lays out exactly as if the
        hidden node were simply absent from the file."""
        with_hidden = layout_screen(screen_from_dict(_screen([
            {"kind": "image", "id": "off", "hidden": True, "file": "nope.png", "size": {"w": 40, "h": 40}},
            {"kind": "image", "id": "on", "file": "nope.png", "size": {"w": 40, "h": 40}},
        ])))
        without = layout_screen(screen_from_dict(_screen([
            {"kind": "image", "id": "on", "file": "nope.png", "size": {"w": 40, "h": 40}},
        ])))
        g1 = with_hidden.root.children[0].geometry
        g2 = without.root.children[0].geometry
        self.assertEqual((g1.x, g1.y, g1.w, g1.h), (g2.x, g2.y, g2.w, g2.h))
        self.assertEqual(len(with_hidden.root.children), 1)

    def test_hidden_image_contributes_no_schema_and_no_baked_array(self) -> None:
        screen = screen_from_dict(_screen([
            {"kind": "image", "id": "on", "file": "nope.png", "size": {"w": 8, "h": 8}},
            {"kind": "image", "id": "off", "hidden": True, "size": {"w": 8, "h": 8},
             "file": "nope.png",
             "bind": {"message": "pwm", "field": "ch0_disabled_icon", "type": "string"}},
        ]))
        layout_screen(screen)
        self.assertNotIn("ch0_disabled_icon", emit_harpia(App(screens=[screen])))
        c_src = emit_screen(screen)
        self.assertNotIn("_off_px", c_src)
        self.assertNotIn('"off"', c_src)


if __name__ == "__main__":
    unittest.main()
