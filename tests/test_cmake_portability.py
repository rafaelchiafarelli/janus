"""windows_build epic task 1: no CMakeLists/template hard-codes GCC-only
warning flags (MSVC rejects -Wextra with a hard D8021 error)."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_SKIP_PARTS = {".venv", "build", "__pycache__", ".git", "janus.egg-info"}
_DEFINITION = "$<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall>"


def _cmake_files() -> list[Path]:
    out = []
    for pattern in ("CMakeLists.txt", "*CMakeLists.txt.tmpl"):
        for p in ROOT.rglob(pattern):
            if not _SKIP_PARTS & set(p.relative_to(ROOT).parts):
                out.append(p)
    return out


class TestCmakeWarningFlags(unittest.TestCase):
    def test_no_literal_gcc_only_flags_outside_the_shared_variable(self) -> None:
        files = _cmake_files()
        self.assertGreaterEqual(len(files), 5)  # the search itself must not silently find nothing
        for p in files:
            for n, line in enumerate(p.read_text().splitlines(), 1):
                if line.lstrip().startswith("#"):
                    continue
                if "-Wall" in line or "-Wextra" in line:
                    self.assertTrue(
                        "-Wall>" in line and _DEFINITION in line or "-Wextra>" in line and "C_COMPILER_ID:MSVC" in line,
                        f"{p.relative_to(ROOT)}:{n}: hard-coded GCC-only flag: {line.strip()}",
                    )

    def test_every_file_that_sets_flags_defines_the_variable(self) -> None:
        for p in _cmake_files():
            text = p.read_text()
            if "JANUS_WARN_FLAGS" in text.replace("set(JANUS_WARN_FLAGS", "", 1):
                self.assertIn("set(JANUS_WARN_FLAGS", text, str(p))


if __name__ == "__main__":
    unittest.main()
