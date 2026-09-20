import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from janus.cli import generate
from janus.ir import App
from janus.stage8_scaffold.scaffold_cmake import render_desktop_cmake, scaffold_desktop_cmake

FIXTURES = Path(__file__).parent / "fixtures"


class TestDesktopCmakeTemplate(unittest.TestCase):
    def test_contract(self) -> None:
        out = render_desktop_cmake()
        self.assertIn("JANUS_GENERATED_DIR", out)
        self.assertIn("FATAL_ERROR", out)
        self.assertIn("file(GLOB JANUS_GENERATED_SOURCES", out)
        self.assertIn("src/*.gen.c", out)
        self.assertIn("janus_desktop_driver janus_runtime", out)
        self.assertIn("JANUS_RUNTIME_BUILD_TESTS OFF", out)
        for local in ("main.c", "janus_actions.c", "desktop_input.c"):
            self.assertIn(local, out)


class TestScaffoldDesktopCmake(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)

    def test_once_only(self) -> None:
        path = Path(self._tmp.name) / "CMakeLists.txt"
        self.assertTrue(scaffold_desktop_cmake(App(screens=[]), path))
        path.write_text("# mine\n")
        self.assertFalse(scaffold_desktop_cmake(App(screens=[]), path))
        self.assertEqual(path.read_text(), "# mine\n")

    def test_generate_writes_it_for_desktop_only(self) -> None:
        src = Path(self._tmp.name) / "src"
        written = generate(FIXTURES / "app.yaml", Path(self._tmp.name) / "gen", scaffold_src=src)
        self.assertIn(src / "desktop" / "CMakeLists.txt", written)
        self.assertFalse((src / "embedded_c" / "CMakeLists.txt").exists())


@unittest.skipUnless(
    shutil.which("cmake") and shutil.which("pkg-config")
    and subprocess.run(["pkg-config", "--exists", "sdl2"]).returncode == 0,
    "needs cmake + SDL2 dev files",
)
class TestScaffoldedTreeBuilds(unittest.TestCase):
    def test_configures_and_builds_with_only_generated_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            generate(FIXTURES / "app.yaml", tmp / "gen", scaffold_src=tmp / "src")
            gen, src, build = tmp / "gen" / "desktop", tmp / "src" / "desktop", tmp / "build"
            for cmd in (
                ["cmake", "-S", str(src), "-B", str(build), f"-DJANUS_GENERATED_DIR={gen}"],
                ["cmake", "--build", str(build), "-j"],
            ):
                r = subprocess.run(cmd, capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertTrue((build / "janus_desktop_app").exists())

    def test_relative_generated_dir_is_relative_to_the_scaffold_folder(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            generate(FIXTURES / "app.yaml", tmp / "gen", scaffold_src=tmp / "src")
            src = tmp / "src" / "desktop"
            # run from an unrelated cwd: only "relative to the scaffold folder" can resolve this
            rel = Path(__import__("os").path.relpath(tmp / "gen" / "desktop", src))
            r = subprocess.run(
                ["cmake", "-S", str(src), "-B", str(tmp / "build"), f"-DJANUS_GENERATED_DIR={rel}"],
                capture_output=True, text=True, cwd=tmp,
            )
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_unset_generated_dir_fails_with_a_clear_message(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            generate(FIXTURES / "app.yaml", tmp / "gen", scaffold_src=tmp / "src")
            r = subprocess.run(
                ["cmake", "-S", str(tmp / "src" / "desktop"), "-B", str(tmp / "build")],
                capture_output=True, text=True,
            )
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("JANUS_GENERATED_DIR is not set", r.stderr)


if __name__ == "__main__":
    unittest.main()
