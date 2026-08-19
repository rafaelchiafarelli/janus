import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen
from janus.ir import Binding

FIXTURES = Path(__file__).parent / "fixtures"


class TestParseScreen(unittest.TestCase):
    def test_user_profile(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")

        self.assertEqual(screen.name, "UserProfile")
        self.assertEqual(screen.root.layout, "column")
        self.assertEqual(len(screen.root.children), 2)

        name_label = screen.root.children[0]
        self.assertEqual(name_label.kind, "label")
        self.assertEqual(name_label.id, "name_label")
        self.assertEqual(name_label.bind, Binding(message="user", field="name", type="string"))
        self.assertIsNone(name_label.text)

        row = screen.root.children[1]
        self.assertEqual(row.kind, "row")
        self.assertEqual(len(row.children), 2)

        caption = row.children[0]
        self.assertEqual(caption.kind, "label")
        self.assertEqual(caption.text, "Battery:")
        self.assertIsNone(caption.bind)

        bar = row.children[1]
        self.assertEqual(bar.kind, "progress")
        self.assertEqual(bar.id, "battery_bar")
        self.assertEqual(bar.bind, Binding(message="user", field="battery_level", type="int"))
        self.assertEqual(bar.size, (80, 12))
        self.assertIsNone(bar.geometry)  # layout pass hasn't run yet


if __name__ == "__main__":
    unittest.main()
