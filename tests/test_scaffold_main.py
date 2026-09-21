import tempfile
import unittest
from pathlib import Path

from janus.ir import App, DisplayConfig
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
        self.assertIn("janus_render_screen(janus_app_get_screen(&janus_app, janus_app.active_screen));", out)
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

    def test_touch_checks_the_nav_strip_before_screen_content(self) -> None:
        out = render_main_c()
        self.assertIn("janus_render_nav_bar(&janus_app);", out)
        self.assertIn("janus_nav_hit_test(&janus_app, x, y)", out)
        # nav check comes before the per-screen hit-test
        self.assertLess(out.index("janus_nav_hit_test"), out.index("janus_touch_hit_test(screen"))

    def test_renders_status_bar_at_startup(self) -> None:
        # status_bar epic task 2 — same "no-op if app.yaml has no status:"
        # unconditional call janus_render_nav_bar already gets
        out = render_main_c()
        self.assertIn("janus_render_status_bar(&janus_app);", out)


class TestRenderMainCEncoder(unittest.TestCase):
    def test_polls_encoder_and_moves_focus(self) -> None:
        out = render_main_c("encoder")
        self.assertIn('#include "janus_input_encoder.h"', out)
        self.assertIn('#include "janus_input_focus.h"', out)
        self.assertIn("janus_encoder_poll(&event, &delta)", out)
        self.assertIn("janus_focus_move(&janus_app, delta);", out)
        self.assertIn("janus_focus_activate(&janus_app)", out)
        self.assertIn("case JANUS_INPUT_ACTION:", out)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(render_main_c("encoder")))


class TestRenderMainCButtons(unittest.TestCase):
    def test_polls_buttons_and_moves_focus(self) -> None:
        out = render_main_c("buttons")
        self.assertIn('#include "janus_input_buttons.h"', out)
        self.assertIn('#include "janus_input_focus.h"', out)
        self.assertIn("janus_buttons_poll(&event)", out)
        self.assertIn("janus_focus_move(&janus_app, 1);", out)
        self.assertIn("janus_focus_move(&janus_app, -1);", out)
        self.assertIn("janus_focus_activate(&janus_app)", out)

    def test_braces_balance(self) -> None:
        self.assertTrue(_balanced_braces(render_main_c("buttons")))


class TestRenderMainCNonBlocking(unittest.TestCase):
    def test_touch_uses_async_render_entry_points(self) -> None:
        out = render_main_c("touch", "non_blocking")
        self.assertIn("janus_render_screen_async_start(janus_app_get_screen(&janus_app, janus_app.active_screen));", out)
        self.assertIn("janus_render_poll();", out)
        self.assertIn("janus_switch_screen_async_start(&janus_app, (uint16_t)hit.navigate_target);", out)
        self.assertNotIn("janus_render_screen(janus_app_get_screen(&janus_app, janus_app.active_screen));", out)

    def test_encoder_and_buttons_also_use_async_render_entry_points(self) -> None:
        for modality in ("encoder", "buttons"):
            out = render_main_c(modality, "non_blocking")
            self.assertIn(
                "janus_render_screen_async_start(janus_app_get_screen(&janus_app, janus_app.active_screen));",
                out,
            )
            self.assertIn("janus_render_poll();", out)
            self.assertIn("janus_switch_screen_async_start(&janus_app, (uint16_t)hit.navigate_target);", out)

    def test_braces_balance(self) -> None:
        for modality in ("touch", "encoder", "buttons"):
            self.assertTrue(_balanced_braces(render_main_c(modality, "non_blocking")))


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

    def test_scaffolds_blocking_when_no_display_declared(self) -> None:
        scaffold_main_c(self.app, self.path)
        self.assertIn("janus_render_screen(janus_app_get_screen(&janus_app, janus_app.active_screen));", self.path.read_text())

    def test_scaffolds_non_blocking_when_the_display_declares_it(self) -> None:
        app = App(screens=[], display=DisplayConfig(width=240, height=320, color="mono", render_mode="non_blocking"))
        scaffold_main_c(app, self.path)
        self.assertIn("janus_render_screen_async_start", self.path.read_text())


class TestDesktopTarget(unittest.TestCase):
    def test_one_desktop_template_per_modality(self) -> None:
        for modality, poll in (("touch", "janus_touch_poll"), ("encoder", "janus_encoder_poll"), ("buttons", "janus_buttons_poll")):
            out = render_main_c(modality, "blocking", "desktop")
            self.assertIn(poll, out)
            self.assertIn("while (janus_desktop_driver_pump())", out)
            self.assertIn("janus_desktop_driver_init(JANUS_DISPLAY_WIDTH, JANUS_DISPLAY_HEIGHT", out)
            self.assertIn("janus_desktop_driver_shutdown();", out)
            self.assertNotIn("display_driver_init", out)
            self.assertTrue(_balanced_braces(out))

    def test_desktop_main_defines_sdl_main_handled_before_including_sdl(self) -> None:
        for modality in ("touch", "encoder", "buttons"):
            out = render_main_c(modality, "blocking", "desktop")
            self.assertIn("#define SDL_MAIN_HANDLED", out)
            self.assertLess(out.index("#define SDL_MAIN_HANDLED"), out.index("#include <SDL.h>"))

    def test_desktop_ignores_render_mode(self) -> None:
        for modality in ("touch", "encoder", "buttons"):
            self.assertEqual(
                render_main_c(modality, "non_blocking", "desktop"),
                render_main_c(modality, "blocking", "desktop"),
            )
        self.assertNotIn("async_start", render_main_c("touch", "non_blocking", "desktop"))

    def test_embedded_c_output_unchanged_by_the_target_argument(self) -> None:
        for modality in ("touch", "encoder", "buttons"):
            for mode in ("blocking", "non_blocking"):
                self.assertEqual(render_main_c(modality, mode), render_main_c(modality, mode, "embedded_c"))

    def test_scaffold_main_c_picks_the_desktop_template_and_stays_once_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "main.c"
            app = App(screens=[], input_modality="encoder")
            self.assertTrue(scaffold_main_c(app, path, "desktop"))
            self.assertIn("janus_desktop_driver_pump", path.read_text())
            path.write_text("/* mine */\n")
            self.assertFalse(scaffold_main_c(app, path, "desktop"))
            self.assertEqual(path.read_text(), "/* mine */\n")


if __name__ == "__main__":
    unittest.main()
