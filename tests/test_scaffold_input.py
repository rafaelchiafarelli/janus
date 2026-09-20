import tempfile
import unittest
from pathlib import Path

from janus.cli import generate
from janus.ir import App
from janus.stage8_scaffold.scaffold_input import render_desktop_input_c, scaffold_desktop_input_c

FIXTURES = Path(__file__).parent / "fixtures"


class TestRenderDesktopInputC(unittest.TestCase):
    def test_each_modality_defines_its_poll_function(self) -> None:
        for modality, poll in (
            ("touch", "bool janus_touch_poll("),
            ("encoder", "bool janus_encoder_poll("),
            ("buttons", "bool janus_buttons_poll("),
        ):
            out = render_desktop_input_c(modality)
            self.assertIn(poll, out)
            # state polling only — the pump owns the event queue
            self.assertNotIn("SDL_PollEvent", out)
            self.assertNotIn("SDL_PeepEvents", out)

    def test_default_mappings(self) -> None:
        touch = render_desktop_input_c("touch")
        self.assertIn("SDL_GetMouseState", touch)
        self.assertIn("SDL_BUTTON_LEFT", touch)
        enc = render_desktop_input_c("encoder")
        for key in ("SDL_SCANCODE_LEFT", "SDL_SCANCODE_RIGHT", "SDL_SCANCODE_RETURN"):
            self.assertIn(key, enc)
        self.assertIn("JANUS_ENCODER_CLICK", enc)
        btn = render_desktop_input_c("buttons")
        for ev in ("JANUS_BUTTON_PREV", "JANUS_BUTTON_NEXT", "JANUS_BUTTON_SELECT"):
            self.assertIn(ev, btn)


class TestScaffoldDesktopInputC(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.path = Path(self._tmp.name) / "desktop_input.c"

    def test_scaffolds_the_declared_modality_once(self) -> None:
        app = App(screens=[], input_modality="buttons")
        self.assertTrue(scaffold_desktop_input_c(app, self.path))
        self.assertIn("janus_buttons_poll", self.path.read_text())

    def test_never_touches_an_existing_file(self) -> None:
        self.path.write_text("/* my own mapping */\n")
        self.assertFalse(scaffold_desktop_input_c(App(screens=[]), self.path))
        self.assertEqual(self.path.read_text(), "/* my own mapping */\n")


class TestGenerateScaffoldsDesktopInputOnly(unittest.TestCase):
    def test_desktop_only_and_once(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "src"
            written = generate(FIXTURES / "app.yaml", Path(tmp) / "gen", scaffold_src=src)
            desktop_input = src / "desktop" / "desktop_input.c"
            self.assertTrue(desktop_input.exists())
            self.assertIn(desktop_input, written)
            self.assertFalse((src / "embedded_c" / "desktop_input.c").exists())

            desktop_input.write_text("/* hand-edited */\n")
            generate(FIXTURES / "app.yaml", Path(tmp) / "gen", scaffold_src=src)
            self.assertEqual(desktop_input.read_text(), "/* hand-edited */\n")


if __name__ == "__main__":
    unittest.main()
