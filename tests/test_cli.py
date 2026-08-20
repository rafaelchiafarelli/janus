import tempfile
import unittest
from pathlib import Path

from janus.cli import generate, main

FIXTURES = Path(__file__).parent / "fixtures"


class TestGenerate(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.target_dir = Path(self._tmp.name) / "generated"

    def test_writes_every_expected_file_from_a_real_app_yaml(self) -> None:
        generate(FIXTURES / "app.yaml", self.target_dir)
        expected = {
            "userprofile_screen.gen.h",
            "userprofile_screen.gen.c",
            "boxdemo_screen.gen.h",
            "boxdemo_screen.gen.c",
            "janus_actions.gen.h",
            "janus_app.gen.c",
            "janus_bindings.gen.h",
            "janus_bindings.gen.c",
            "janus_generated.harpia",
        }
        actual = {p.name for p in self.target_dir.iterdir()}
        self.assertEqual(expected, actual)

    def test_second_run_with_no_changes_writes_nothing(self) -> None:
        generate(FIXTURES / "app.yaml", self.target_dir)
        written = generate(FIXTURES / "app.yaml", self.target_dir)
        self.assertEqual(written, [])

    def test_scaffold_src_writes_human_owned_files_once(self) -> None:
        src_dir = Path(self._tmp.name) / "src"
        written = generate(FIXTURES / "app.yaml", self.target_dir, scaffold_src=src_dir)

        self.assertTrue((src_dir / "main.c").exists())
        self.assertTrue((src_dir / "janus_actions.c").exists())
        self.assertIn(src_dir / "main.c", written)
        self.assertIn(src_dir / "janus_actions.c", written)

        (src_dir / "main.c").write_text("/* hand-edited */\n")
        generate(FIXTURES / "app.yaml", self.target_dir, scaffold_src=src_dir)
        self.assertEqual((src_dir / "main.c").read_text(), "/* hand-edited */\n")

    def test_without_scaffold_src_no_src_files_are_written(self) -> None:
        generate(FIXTURES / "app.yaml", self.target_dir)
        self.assertFalse((Path(self._tmp.name) / "src").exists())


class TestMain(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.target_dir = Path(self._tmp.name) / "generated"

    def test_returns_zero_and_writes_files(self) -> None:
        exit_code = main([str(FIXTURES / "app.yaml"), str(self.target_dir)])
        self.assertEqual(exit_code, 0)
        self.assertTrue((self.target_dir / "janus_generated.harpia").exists())

    def test_missing_app_yaml_is_a_usage_error(self) -> None:
        with self.assertRaises(SystemExit) as ctx:
            main([str(Path(self._tmp.name) / "nope.yaml"), str(self.target_dir)])
        self.assertNotEqual(ctx.exception.code, 0)


if __name__ == "__main__":
    unittest.main()
