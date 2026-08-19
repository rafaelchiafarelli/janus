import tempfile
import unittest
from pathlib import Path

from janus.ir import App, Screen, Widget
from janus.stage5_actions.scaffold_actions import render_actions_c, scaffold_actions_c


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


class TestRenderActionsC(unittest.TestCase):
    def setUp(self) -> None:
        screen = Screen(
            name="One",
            root=Widget(kind="column", id="r1", children=[
                Widget(kind="button", id="reboot_button", text="Reboot", on_press="reboot"),
                Widget(kind="button", id="about_button", text="About", on_press="open_about_dialog"),
                Widget(kind="button", id="goto_two", text="Go", navigate="Two"),
            ]),
        )
        self.app = App(screens=[screen], nav=None)

    def test_one_case_per_on_press_action(self) -> None:
        out = render_actions_c(self.app)
        self.assertIn("case JANUS_ACTION_REBOOT: /* TODO */ break;", out)
        self.assertIn("case JANUS_ACTION_OPEN_ABOUT_DIALOG: /* TODO */ break;", out)

    def test_navigate_does_not_get_a_case(self) -> None:
        out = render_actions_c(self.app)
        self.assertNotIn("GOTO_TWO", out)

    def test_has_default_and_includes_actions_header(self) -> None:
        out = render_actions_c(self.app)
        self.assertIn("default: break;", out)
        self.assertIn('#include "janus_actions.gen.h"', out)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(render_actions_c(self.app)))

    def test_no_actions_still_renders_valid_skeleton(self) -> None:
        empty_app = App(screens=[Screen(name="Empty", root=Widget(kind="column", id="r"))], nav=None)
        out = render_actions_c(empty_app)
        self.assertIn("void janus_handle_action(janus_action_t action) {", out)
        self.assertTrue(_balanced_braces(out))


class TestScaffoldActionsC(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.path = Path(self._tmp.name) / "src" / "janus_actions.c"
        screen = Screen(
            name="One",
            root=Widget(kind="column", id="r1", children=[
                Widget(kind="button", id="reboot_button", text="Reboot", on_press="reboot"),
            ]),
        )
        self.app = App(screens=[screen], nav=None)

    def test_scaffolds_when_missing(self) -> None:
        self.assertTrue(scaffold_actions_c(self.app, self.path))
        self.assertTrue(self.path.exists())
        self.assertIn("JANUS_ACTION_REBOOT", self.path.read_text())

    def test_never_touches_an_existing_file(self) -> None:
        self.path.parent.mkdir(parents=True)
        self.path.write_text("/* hand-written, do not overwrite */\n")

        self.assertFalse(scaffold_actions_c(self.app, self.path))
        self.assertEqual(self.path.read_text(), "/* hand-written, do not overwrite */\n")


if __name__ == "__main__":
    unittest.main()
