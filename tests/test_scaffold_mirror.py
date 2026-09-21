import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from janus.cli import generate
from janus.ir import App
from janus.stage8_scaffold.scaffold_cmake import render_desktop_cmake
from janus.stage8_scaffold.scaffold_input import scaffold_mirror_link_c
from janus.stage8_scaffold.scaffold_main import render_main_c

FIXTURES = Path(__file__).parent / "fixtures"


class TestMirrorMain(unittest.TestCase):
    def test_shape(self) -> None:
        out = render_main_c("encoder", "blocking", "desktop", mirror=True)
        self.assertLess(out.index("#define SDL_MAIN_HANDLED"), out.index("#include <SDL.h>"))
        self.assertIn("while (janus_desktop_driver_pump())", out)
        self.assertIn("mirror_link_poll(&janus_app, &state)", out)
        self.assertIn("janus_remote_state_apply(&janus_app, &state)", out)
        self.assertIn("janus_render_screen_if_dirty", out)
        self.assertIn("janus_desktop_driver_shutdown();", out)

    def test_has_no_input_path_at_all(self) -> None:
        out = render_main_c("touch", "blocking", "desktop", mirror=True)
        for forbidden in (
            "janus_touch_poll", "janus_encoder_poll", "janus_buttons_poll",
            "janus_focus_move", "janus_focus_activate", "janus_switch_screen",
            "janus_handle_action", "janus_toggle_box", "janus_nav_hit_test",
        ):
            self.assertNotIn(forbidden, out)

    def test_ignores_modality_and_render_mode(self) -> None:
        base = render_main_c("touch", "blocking", "desktop", mirror=True)
        for modality in ("encoder", "buttons"):
            for mode in ("blocking", "non_blocking"):
                self.assertEqual(render_main_c(modality, mode, "desktop", mirror=True), base)

    def test_mirror_is_desktop_only(self) -> None:
        with self.assertRaises(ValueError):
            render_main_c("touch", "blocking", "embedded_c", mirror=True)

    def test_default_output_is_unchanged_by_the_new_parameter(self) -> None:
        for target in ("embedded_c", "desktop"):
            self.assertEqual(render_main_c("encoder", "blocking", target),
                             render_main_c("encoder", "blocking", target, mirror=False))


class TestMirrorCmake(unittest.TestCase):
    def test_input_source_token_is_always_filled(self) -> None:
        normal, mirror = render_desktop_cmake(), render_desktop_cmake(mirror=True)
        self.assertNotIn("@INPUT_SOURCE@", normal + mirror)
        self.assertIn("desktop_input.c", normal)
        self.assertNotIn("mirror_link.c", normal)
        self.assertIn("mirror_link.c", mirror)
        self.assertNotIn("desktop_input.c", mirror)

    def test_committed_demo_scaffold_still_matches_the_default_render(self) -> None:
        demo = (Path(__file__).parent.parent / "examples" / "desktop_demo" / "src" / "CMakeLists.txt").read_text()
        self.assertEqual(demo, render_desktop_cmake())


class TestMirrorLink(unittest.TestCase):
    def test_once_only_stub(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "mirror_link.c"
            self.assertTrue(scaffold_mirror_link_c(App(screens=[]), path))
            text = path.read_text()
            self.assertIn("bool mirror_link_poll(janus_app_t *app, janus_remote_state_t *out)", text)
            self.assertIn("return false;", text)
            path.write_text("/* mine */\n")
            self.assertFalse(scaffold_mirror_link_c(App(screens=[]), path))
            self.assertEqual(path.read_text(), "/* mine */\n")


class TestGenerateMirror(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)

    def test_desktop_gets_mirror_scaffolds_and_no_input_file(self) -> None:
        written = generate(FIXTURES / "app.yaml", self.tmp / "gen", scaffold_src=self.tmp / "src", mirror=True)
        d = self.tmp / "src" / "desktop"
        self.assertIn("janus_remote_state_apply", (d / "main.c").read_text())
        self.assertTrue((d / "mirror_link.c").exists())
        self.assertIn(d / "mirror_link.c", written)
        self.assertFalse((d / "desktop_input.c").exists())
        self.assertIn("mirror_link.c", (d / "CMakeLists.txt").read_text())
        self.assertNotIn("desktop_input.c", (d / "CMakeLists.txt").read_text())

    def test_embedded_c_is_untouched_by_mirror(self) -> None:
        generate(FIXTURES / "app.yaml", self.tmp / "g1", scaffold_src=self.tmp / "s1", mirror=True)
        generate(FIXTURES / "app.yaml", self.tmp / "g2", scaffold_src=self.tmp / "s2")
        self.assertEqual((self.tmp / "s1" / "embedded_c" / "main.c").read_text(),
                         (self.tmp / "s2" / "embedded_c" / "main.c").read_text())
        self.assertFalse((self.tmp / "s1" / "embedded_c" / "mirror_link.c").exists())

    def test_default_run_is_unchanged(self) -> None:
        generate(FIXTURES / "app.yaml", self.tmp / "gen", scaffold_src=self.tmp / "src")
        d = self.tmp / "src" / "desktop"
        self.assertTrue((d / "desktop_input.c").exists())
        self.assertFalse((d / "mirror_link.c").exists())


@unittest.skipUnless(
    shutil.which("cmake") and shutil.which("pkg-config")
    and subprocess.run(["pkg-config", "--exists", "sdl2"]).returncode == 0,
    "needs cmake + SDL2 dev files",
)
class TestMirrorTreeBuilds(unittest.TestCase):
    def test_mirror_scaffold_configures_and_builds_with_only_generated_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            generate(FIXTURES / "app.yaml", tmp / "gen", scaffold_src=tmp / "src", mirror=True)
            for cmd in (
                ["cmake", "-S", str(tmp / "src" / "desktop"), "-B", str(tmp / "build"),
                 f"-DJANUS_GENERATED_DIR={tmp / 'gen' / 'desktop'}"],
                ["cmake", "--build", str(tmp / "build"), "-j"],
            ):
                r = subprocess.run(cmd, capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertTrue((tmp / "build" / "janus_desktop_app").exists())


if __name__ == "__main__":
    unittest.main()
