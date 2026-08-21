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

    def test_static_text_widget_gets_its_text_baked_in(self) -> None:
        idx = self.out.index('.id = "battery_caption"')
        segment = self.out[idx: idx + 200]
        self.assertIn('.static_text = "Battery:",', segment)

    def test_bound_widget_has_null_static_text(self) -> None:
        idx = self.out.index('.id = "name_label"')
        segment = self.out[idx: idx + 200]
        self.assertIn(".static_text = NULL,", segment)


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


class TestEmitEmbeddedCLowEffortKinds(unittest.TestCase):
    """divider/toggle/badge/slider — added on top of Stage 4/6, each
    reusing an existing bind shape (no new IR fields, no new runtime
    state)."""

    def setUp(self) -> None:
        screen = layout_screen(Screen(
            name="Kinds",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="divider", id="d"),
                Widget(kind="toggle", id="t", bind=Binding(message="dev", field="on", type="int")),
                Widget(
                    kind="badge", id="b", size=(8, 8),
                    bind=Binding(message="dev", field="flag", type="int"),
                ),
                Widget(
                    kind="slider", id="s", size=(60, 10), range=(0, 100),
                    bind=Binding(message="dev", field="level", type="int"),
                ),
            ]),
        ))
        self.out = emit_screen(screen)

    def test_each_kind_maps_to_its_own_enum_value(self) -> None:
        self.assertIn("JANUS_WIDGET_DIVIDER", self.out)
        self.assertIn("JANUS_WIDGET_TOGGLE", self.out)
        self.assertIn("JANUS_WIDGET_BADGE", self.out)
        self.assertIn("JANUS_WIDGET_SLIDER", self.out)

    def test_toggle_and_badge_reuse_checkbox_style_int_bind(self) -> None:
        self.assertIn("offsetof(dev_t, on)", self.out)
        self.assertIn("offsetof(dev_t, flag)", self.out)

    def test_slider_reuses_progress_style_bind_with_range(self) -> None:
        self.assertIn("offsetof(dev_t, level)", self.out)
        idx = self.out.index('.id = "s"')
        segment = self.out[idx: idx + 300]
        self.assertIn(".range_min = 0, .range_max = 100", segment)


class TestEmitEmbeddedCFocusOrder(unittest.TestCase):
    """Stage 6: .focus_order is baked at generation time, in the same
    pre-order traversal janus_runtime.c/janus_input_touch.c walk at
    runtime — not the post-order _emit_widget uses to emit C (that one's
    for forward declarations only, see emit_embedded_c.py)."""

    def _segment(self, out: str, widget_id: str) -> str:
        idx = out.index(f'.id = "{widget_id}"')
        return out[idx: idx + 400]

    def test_button_with_on_press_is_focusable(self) -> None:
        screen = Screen(
            name="Focus",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="label", id="lbl", text="Hi"),
                Widget(kind="button", id="btn", text="Go", on_press="reboot"),
            ]),
        )
        out = emit_screen(layout_screen(screen))
        self.assertIn(".focus_order = 255", self._segment(out, "lbl"))
        self.assertIn(".focus_order = 0", self._segment(out, "btn"))

    def test_button_with_navigate_is_focusable(self) -> None:
        screen = Screen(
            name="Focus",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="button", id="btn", text="Go", navigate="Other"),
            ]),
        )
        idx = {"Focus": 0, "Other": 1}
        out = emit_screen(layout_screen(screen), idx)
        self.assertIn(".focus_order = 0", self._segment(out, "btn"))

    def test_box_is_focusable_but_its_children_are_not(self) -> None:
        screen = Screen(
            name="Focus",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="box", id="box1", layout="column", children=[
                    Widget(kind="checkbox", id="chk",
                           bind=Binding(message="dev", field="on", type="int")),
                ]),
            ]),
        )
        out = emit_screen(layout_screen(screen))
        self.assertIn(".focus_order = 0", self._segment(out, "box1"))
        self.assertIn(".focus_order = 255", self._segment(out, "chk"))

    def test_order_follows_traversal_order_not_c_emission_order(self) -> None:
        """box1 has a focusable child array emitted *before* box1's own
        initializer in the generated C (post-order, for forward
        declarations) — but box1 itself must still get a lower
        focus_order than the button that comes after it in the YAML,
        since focus order follows the screen's visual/traversal order."""
        screen = Screen(
            name="Focus",
            root=Widget(kind="column", id="root", children=[
                Widget(kind="box", id="box1", layout="column", children=[
                    Widget(kind="button", id="inner_btn", text="X", on_press="a"),
                ]),
                Widget(kind="button", id="after_btn", text="Y", on_press="b"),
            ]),
        )
        out = emit_screen(layout_screen(screen))
        self.assertIn(".focus_order = 0", self._segment(out, "box1"))
        self.assertIn(".focus_order = 1", self._segment(out, "inner_btn"))
        self.assertIn(".focus_order = 2", self._segment(out, "after_btn"))


if __name__ == "__main__":
    unittest.main()
