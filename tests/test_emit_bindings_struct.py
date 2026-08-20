import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.stage3b_embedded_c.emit_bindings_struct import emit_bindings_header, emit_bindings_source
from janus.ir import App, Binding, Screen, Widget

FIXTURES = Path(__file__).parent / "fixtures"


class TestEmitBindingsStruct(unittest.TestCase):
    def test_user_profile_screen_header(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        out = emit_bindings_header(App(screens=[screen]))
        self.assertEqual(
            out,
            "typedef struct {\n"
            "    const char * name;\n"
            "    int battery_level;\n"
            "} user_t;\n\n"
            "extern user_t user_instance;\n",
        )

    def test_user_profile_screen_source_zero_inits(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        out = emit_bindings_source(App(screens=[screen]))
        self.assertEqual(out, "user_t user_instance = {0};\n")

    def test_dedups_same_field_bound_by_two_widgets(self) -> None:
        widget_a = Widget(kind="label", id="a", bind=Binding("device", "name", "string"))
        widget_b = Widget(kind="header", id="b", bind=Binding("device", "name", "string"))
        root = Widget(kind="column", id="root", children=[widget_a, widget_b])
        out = emit_bindings_header(App(screens=[Screen(name="S", root=root)]))
        self.assertEqual(out.count("name;"), 1)

    def test_merges_across_screens(self) -> None:
        s1 = Screen(name="One", root=Widget(kind="column", id="r1", children=[
            Widget(kind="label", id="a", bind=Binding("device", "name", "string")),
        ]))
        s2 = Screen(name="Two", root=Widget(kind="column", id="r2", children=[
            Widget(kind="label", id="b", bind=Binding("settings", "mode", "int")),
        ]))
        out = emit_bindings_header(App(screens=[s1, s2]))
        self.assertIn("device_t", out)
        self.assertIn("settings_t", out)

    def test_int64_field_adds_stdint_include(self) -> None:
        widget = Widget(kind="label", id="a", bind=Binding("device", "uptime", "int64"))
        root = Widget(kind="column", id="root", children=[widget])
        out = emit_bindings_header(App(screens=[Screen(name="S", root=root)]))
        self.assertIn("#include <stdint.h>", out)
        self.assertIn("int64_t uptime;", out)

    def test_no_int64_field_omits_stdint_include(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        out = emit_bindings_header(App(screens=[screen]))
        self.assertNotIn("stdint.h", out)

    def test_no_bindings_at_all_is_empty(self) -> None:
        root = Widget(kind="column", id="root", children=[Widget(kind="label", id="a", text="hi")])
        app = App(screens=[Screen(name="S", root=root)])
        self.assertEqual(emit_bindings_header(app), "")
        self.assertEqual(emit_bindings_source(app), "")


if __name__ == "__main__":
    unittest.main()
