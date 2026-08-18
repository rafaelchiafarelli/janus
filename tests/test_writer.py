import unittest
import tempfile
from pathlib import Path

from janus.writer import write_if_changed


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


if __name__ == "__main__":
    unittest.main()
