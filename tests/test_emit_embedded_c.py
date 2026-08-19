import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.stage3b_embedded_c.emit_embedded_c import emit_screen
from janus.stage2_layout.layout import layout_screen
from janus.ir import Binding, Screen, Widget

FIXTURES = Path(__file__).parent / "fixtures"


def _balanced_braces(text: str) -> bool:
    depth = 0
    for ch in text:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


class TestEmitEmbeddedC(unittest.TestCase):
    def setUp(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        self.screen = layout_screen(screen)
        self.out = emit_screen(self.screen)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(self.out))

    def test_top_level_widgets_array_has_both_children(self) -> None:
        self.assertIn("static const janus_widget_desc_t userprofile_widgets[] = {", self.out)
        self.assertIn(".widget_count = 2,", self.out)

    def test_nested_row_gets_its_own_array_emitted_before_use(self) -> None:
        array_decl = self.out.index("static const janus_widget_desc_t userprofile_arr1[]")
        array_use = self.out.index(".children = userprofile_arr1")
        self.assertLess(array_decl, array_use, "child array must be declared before it's referenced")

    def test_leaf_bind_uses_offsetof(self) -> None:
        self.assertIn("offsetof(user_t, name)", self.out)
        self.assertIn("offsetof(user_t, battery_level)", self.out)
        self.assertIn("JANUS_FIELD_STRING", self.out)
        self.assertIn("JANUS_FIELD_INT,", self.out)

    def test_static_label_has_no_bind(self) -> None:
        self.assertIn('.id = "battery_caption"', self.out)
        # the static caption's own entry has no offsetof call right after its id
        idx = self.out.index('.id = "battery_caption"')
        segment = self.out[idx: idx + 200]
        self.assertIn("JANUS_FIELD_NONE", segment)

    def test_geometry_baked_in(self) -> None:
        self.assertIn(".geometry = {0, 0, 60, 12}", self.out)  # name_label
        self.assertIn(".geometry = {64, 16, 80, 12}", self.out)  # battery_bar

    def test_screen_desc_name(self) -> None:
        self.assertIn('.name = "UserProfile",', self.out)

    def test_bound_struct_points_at_the_single_message_instance(self) -> None:
        self.assertIn(".bound_struct = &user_instance,", self.out)


class TestEmitEmbeddedCBoundStruct(unittest.TestCase):
    def test_no_bindings_means_null_bound_struct(self) -> None:
        screen = layout_screen(Screen(
            name="Empty",
            root=Widget(kind="column", id="r", children=[
                Widget(kind="label", id="l", text="hi"),
            ]),
        ))
        out = emit_screen(screen)
        self.assertIn(".bound_struct = NULL,", out)

    def test_more_than_one_bound_message_raises(self) -> None:
        screen = layout_screen(Screen(
            name="TwoMessages",
            root=Widget(kind="column", id="r", children=[
                Widget(kind="label", id="a", bind=Binding(message="foo", field="x", type="string")),
                Widget(kind="label", id="b", bind=Binding(message="bar", field="y", type="string")),
            ]),
        ))
        with self.assertRaises(ValueError):
            emit_screen(screen)


class TestEmitEmbeddedCBox(unittest.TestCase):
    """Closes the gap flagged after the Stage 3b slice: box's dual
    geometry (expanded vs collapsed) was only ever tested through
    layout.py, never through the emitter that actually bakes it into C.
    """

    def setUp(self) -> None:
        screen = parse_screen(FIXTURES / "box_demo.screen.yaml")
        self.screen = layout_screen(screen)
        self.out = emit_screen(self.screen)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(self.out))

    def test_box_kind_and_both_geometries_present(self) -> None:
        self.assertIn("JANUS_WIDGET_BOX", self.out)
        self.assertIn(".geometry = {0, 0, 10, 26}", self.out)            # header + body
        self.assertIn(".geometry_collapsed = {0, 0, 10, 16}", self.out)  # header only

    def test_led_child_offset_below_header_and_bound(self) -> None:
        self.assertIn(".geometry = {0, 16, 10, 10}", self.out)  # status_led, below BOX_HEADER_H
        self.assertIn("offsetof(dev_t, status)", self.out)

    def test_box_default_expanded_true_is_baked_in(self) -> None:
        idx = self.out.index('.id = "net_box"')
        segment = self.out[idx: idx + 300]
        self.assertIn(".initial_expanded = true", segment)


class TestEmitEmbeddedCBoxCollapsedByDefault(unittest.TestCase):
    def setUp(self) -> None:
        screen = layout_screen(Screen(
            name="Collapsed",
            root=Widget(kind="column", id="r", children=[
                Widget(
                    kind="box", id="settings_box", layout="column",
                    collapsible=True, default_expanded=False,
                    children=[Widget(kind="label", id="l", text="hi")],
                ),
            ]),
        ))
        self.out = emit_screen(screen)

    def test_initial_expanded_false_is_baked_in(self) -> None:
        idx = self.out.index('.id = "settings_box"')
        segment = self.out[idx: idx + 300]
        self.assertIn(".initial_expanded = false", segment)


if __name__ == "__main__":
    unittest.main()
