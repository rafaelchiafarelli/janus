import unittest
import tempfile
from pathlib import Path

from janus.writer import copy_tree_if_changed, write_if_changed


class TestWriteIfChanged(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = Path(self._tmp.name)

    def test_creates_file_and_reports_write(self) -> None:
        path = self.dir / "out.txt"
        self.assertTrue(write_if_changed(path, "hello"))
        self.assertEqual(path.read_text(), "hello")

    def test_creates_missing_parent_dirs(self) -> None:
        path = self.dir / "nested" / "deeper" / "out.txt"
        self.assertTrue(write_if_changed(path, "hello"))
        self.assertEqual(path.read_text(), "hello")

    def test_identical_content_is_a_no_op(self) -> None:
        path = self.dir / "out.txt"
        write_if_changed(path, "hello")
        mtime_before = path.stat().st_mtime_ns
        self.assertFalse(write_if_changed(path, "hello"))
        self.assertEqual(path.stat().st_mtime_ns, mtime_before)

    def test_changed_content_is_written(self) -> None:
        path = self.dir / "out.txt"
        write_if_changed(path, "hello")
        self.assertTrue(write_if_changed(path, "goodbye"))
        self.assertEqual(path.read_text(), "goodbye")


class TestCopyTreeIfChanged(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.src = Path(self._tmp.name) / "src"
        self.dest = Path(self._tmp.name) / "dest"
        (self.src / "nested").mkdir(parents=True)
        (self.src / "top.txt").write_text("top")
        (self.src / "nested" / "deep.txt").write_text("deep")

    def test_copies_every_file_preserving_layout(self) -> None:
        written = copy_tree_if_changed(self.src, self.dest)
        self.assertEqual((self.dest / "top.txt").read_text(), "top")
        self.assertEqual((self.dest / "nested" / "deep.txt").read_text(), "deep")
        self.assertEqual(len(written), 2)

    def test_second_run_with_no_changes_writes_nothing(self) -> None:
        copy_tree_if_changed(self.src, self.dest)
        written = copy_tree_if_changed(self.src, self.dest)
        self.assertEqual(written, [])

    def test_changing_one_file_rewrites_only_that_one(self) -> None:
        copy_tree_if_changed(self.src, self.dest)
        (self.src / "top.txt").write_text("changed")
        written = copy_tree_if_changed(self.src, self.dest)
        self.assertEqual(written, [self.dest / "top.txt"])


if __name__ == "__main__":
    unittest.main()
