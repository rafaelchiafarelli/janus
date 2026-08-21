import tempfile
import unittest
from pathlib import Path

from janus.ir import App
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


class TestRenderMainCEncoder(unittest.TestCase):
    def test_polls_encoder_and_moves_focus(self) -> None:
        out = render_main_c("encoder")
        self.assertIn('#include "janus_input_encoder.h"', out)
        self.assertIn('#include "janus_input_focus.h"', out)
        self.assertIn("janus_encoder_poll(&event, &delta)", out)
        self.assertIn("janus_focus_move(screen, delta);", out)
        self.assertIn("janus_focus_activate(screen)", out)
        self.assertIn("case JANUS_INPUT_ACTION:", out)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(render_main_c("encoder")))


class TestRenderMainCButtons(unittest.TestCase):
    def test_polls_buttons_and_moves_focus(self) -> None:
        out = render_main_c("buttons")
        self.assertIn('#include "janus_input_buttons.h"', out)
        self.assertIn('#include "janus_input_focus.h"', out)
        self.assertIn("janus_buttons_poll(&event)", out)
        self.assertIn("janus_focus_move(screen, 1);", out)
        self.assertIn("janus_focus_move(screen, -1);", out)
        self.assertIn("janus_focus_activate(screen)", out)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(render_main_c("buttons")))


class TestScaffoldMainC(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.path = Path(self._tmp.name) / "src" / "main.c"
        self.app = App(screens=[])

    def test_scaffolds_when_missing(self) -> None:
        self.assertTrue(scaffold_main_c(self.app, self.path))
        self.assertTrue(self.path.exists())
        self.assertIn("int main(void)", self.path.read_text())

    def test_never_touches_an_existing_file(self) -> None:
        self.path.parent.mkdir(parents=True)
        self.path.write_text("/* hand-written, do not overwrite */\n")

        self.assertFalse(scaffold_main_c(self.app, self.path))
        self.assertEqual(self.path.read_text(), "/* hand-written, do not overwrite */\n")

    def test_scaffolds_the_modality_the_app_declares(self) -> None:
        encoder_app = App(screens=[], input_modality="encoder")
        scaffold_main_c(encoder_app, self.path)
        self.assertIn("janus_encoder_poll", self.path.read_text())


if __name__ == "__main__":
    unittest.main()
