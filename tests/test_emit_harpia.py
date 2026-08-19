import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.stage3a_harpia.emit_harpia import emit_harpia
from janus.ir import App, Binding, Screen, Widget

FIXTURES = Path(__file__).parent / "fixtures"


class TestEmitHarpia(unittest.TestCase):
    def test_user_profile_screen(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        out = emit_harpia(App(screens=[screen]))
        self.assertEqual(
            out,
            "message user{\n"
            "    string name;\n"
            "    int battery_level;\n"
            "};\n",
        )

    def test_dedups_same_field_bound_by_two_widgets(self) -> None:
        widget_a = Widget(kind="label", id="a", bind=Binding("device", "name", "string"))
        widget_b = Widget(kind="header", id="b", bind=Binding("device", "name", "string"))
        root = Widget(kind="column", id="root", children=[widget_a, widget_b])
        out = emit_harpia(App(screens=[Screen(name="S", root=root)]))
        self.assertEqual(out.count("string name;"), 1)

    def test_merges_across_screens(self) -> None:
        s1 = Screen(name="One", root=Widget(kind="column", id="r1", children=[
            Widget(kind="label", id="a", bind=Binding("device", "name", "string")),
        ]))
        s2 = Screen(name="Two", root=Widget(kind="column", id="r2", children=[
            Widget(kind="label", id="b", bind=Binding("settings", "mode", "int")),
        ]))
        out = emit_harpia(App(screens=[s1, s2]))
        self.assertIn("message device{", out)
        self.assertIn("message settings{", out)

    def test_conflicting_types_raise(self) -> None:
        widget_a = Widget(kind="label", id="a", bind=Binding("device", "name", "string"))
        widget_b = Widget(kind="progress", id="b", bind=Binding("device", "name", "int"), range=(0, 1))
        root = Widget(kind="column", id="root", children=[widget_a, widget_b])
        with self.assertRaises(ValueError):
            emit_harpia(App(screens=[Screen(name="S", root=root)]))


if __name__ == "__main__":
    unittest.main()
