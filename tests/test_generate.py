import tempfile
import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.generate import write_project
from janus.ir import App, NavTarget
from janus.stage2_layout.layout import layout_screen

FIXTURES = Path(__file__).parent / "fixtures"


class TestWriteProject(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.out_dir = Path(self._tmp.name)

        user_profile = layout_screen(parse_screen(FIXTURES / "user_profile.screen.yaml"))
        box_demo = layout_screen(parse_screen(FIXTURES / "box_demo.screen.yaml"))
        self.app = App(
            screens=[user_profile, box_demo],
            nav=[
                NavTarget(screen="UserProfile", title="Profile"),
                NavTarget(screen="BoxDemo", title="Boxes"),
            ],
        )

    def test_writes_every_expected_file(self) -> None:
        write_project(self.app, self.out_dir)
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
        actual = {p.name for p in self.out_dir.iterdir()}
        self.assertEqual(expected, actual)

    def test_harpia_include_content_matches_emitter(self) -> None:
        from janus.stage3a_harpia.emit_harpia import emit_harpia

        write_project(self.app, self.out_dir)
        content = (self.out_dir / "janus_generated.harpia").read_text()
        self.assertEqual(content, emit_harpia(self.app))
        self.assertIn("message user{", content)
        self.assertIn("message dev{", content)

    def test_screen_c_includes_own_header(self) -> None:
        write_project(self.app, self.out_dir)
        content = (self.out_dir / "userprofile_screen.gen.c").read_text()
        self.assertIn('#include "userprofile_screen.gen.h"', content)

    def test_second_run_with_no_changes_writes_nothing(self) -> None:
        write_project(self.app, self.out_dir)
        paths = list(self.out_dir.iterdir())
        mtimes_before = {p: p.stat().st_mtime_ns for p in paths}

        written = write_project(self.app, self.out_dir)

        self.assertEqual(written, [])
        for p in paths:
            self.assertEqual(p.stat().st_mtime_ns, mtimes_before[p])

    def test_returns_only_paths_actually_written_first_run(self) -> None:
        written = write_project(self.app, self.out_dir)
        self.assertEqual(len(written), 9)


if __name__ == "__main__":
    unittest.main()
