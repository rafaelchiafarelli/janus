import tempfile
import unittest
from pathlib import Path

from janus.stage8_scaffold.scaffold_main import render_main_c, scaffold_main_c


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


class TestRenderMainC(unittest.TestCase):
    def test_boots_driver_then_renders_active_screen(self) -> None:
        out = render_main_c()
        self.assertIn("display_driver_init();", out)
        self.assertIn("janus_render_screen(janus_app.screens[janus_app.active_screen]);", out)
        self.assertIn("int main(void) {", out)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(render_main_c()))

    def test_dispatches_touch_hits_to_action_navigate_and_toggle_box(self) -> None:
        out = render_main_c()
        self.assertIn('#include "janus_input_touch.h"', out)
        self.assertIn("janus_touch_poll(&x, &y)", out)
        self.assertIn("janus_touch_hit_test(screen, x, y)", out)
        self.assertIn("case JANUS_INPUT_ACTION:", out)
        self.assertIn("janus_handle_action((janus_action_t)hit.action);", out)
        self.assertIn("case JANUS_INPUT_NAVIGATE:", out)
        self.assertIn("janus_switch_screen(&janus_app, (uint16_t)hit.navigate_target);", out)
        self.assertIn("case JANUS_INPUT_TOGGLE_BOX:", out)
        self.assertIn("janus_toggle_box(hit.widget);", out)


class TestScaffoldMainC(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.path = Path(self._tmp.name) / "src" / "main.c"

    def test_scaffolds_when_missing(self) -> None:
        self.assertTrue(scaffold_main_c(self.path))
        self.assertTrue(self.path.exists())
        self.assertIn("int main(void)", self.path.read_text())

    def test_never_touches_an_existing_file(self) -> None:
        self.path.parent.mkdir(parents=True)
        self.path.write_text("/* hand-written, do not overwrite */\n")

        self.assertFalse(scaffold_main_c(self.path))
        self.assertEqual(self.path.read_text(), "/* hand-written, do not overwrite */\n")


if __name__ == "__main__":
    unittest.main()
