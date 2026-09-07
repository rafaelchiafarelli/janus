import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import parse_screen, screen_from_dict
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

    def test_fill_defaults_false_and_parses_true(self) -> None:
        screen = screen_from_dict({
            "screen": "Fill",
            "layout": "column",
            "children": [
                {"kind": "label", "id": "a"},
                {"kind": "label", "id": "b", "fill": True},
            ],
        })
        self.assertFalse(screen.root.children[0].fill)
        self.assertTrue(screen.root.children[1].fill)


class TestImageFile(unittest.TestCase):
    def _one_image(self, base_dir, file_value):
        screen = screen_from_dict(
            {
                "screen": "Img",
                "layout": "column",
                "children": [
                    {"kind": "image", "id": "logo", "file": file_value,
                     "size": {"w": 16, "h": 16}},
                ],
            },
            base_dir=base_dir,
        )
        return screen.root.children[0]

    def test_relative_file_is_resolved_against_the_screen_dir(self) -> None:
        img = self._one_image("/proj/screens", "art/logo.png")
        self.assertEqual(img.image_file, "/proj/screens/art/logo.png")

    def test_absolute_file_is_kept_as_is(self) -> None:
        img = self._one_image("/proj/screens", "/assets/logo.png")
        self.assertEqual(img.image_file, "/assets/logo.png")

    def test_no_base_dir_leaves_a_relative_path_relative(self) -> None:
        img = self._one_image(None, "art/logo.png")
        self.assertEqual(img.image_file, "art/logo.png")

    def test_no_file_key_means_none(self) -> None:
        img = self._one_image("/proj/screens", None)
        self.assertIsNone(img.image_file)

    def test_parse_screen_uses_the_yaml_files_own_directory(self) -> None:
        screen = parse_screen(FIXTURES / "user_profile.screen.yaml")
        # nothing in that fixture uses `file:`, but the resolution path
        # still has to run without error over a real on-disk screen.
        self.assertEqual(screen.name, "UserProfile")

    def test_file_on_a_non_image_widget_raises(self) -> None:
        with self.assertRaises(ValueError):
            screen_from_dict({
                "screen": "Bad",
                "layout": "column",
                "children": [{"kind": "label", "id": "x", "file": "logo.png"}],
            })


class TestFormatText(unittest.TestCase):
    def _label(self, **extra):
        data = {
            "screen": "Fmt",
            "layout": "column",
            "children": [{"kind": "label", "id": "l", **extra}],
        }
        return screen_from_dict(data).root.children[0]

    def test_conversion_plus_bind_marks_text_is_format(self) -> None:
        w = self._label(
            text="Duty: %d%%",
            bind={"message": "m", "field": "duty", "type": "int"},
        )
        self.assertTrue(w.text_is_format)

    def test_float_precision_conversion_is_recognised(self) -> None:
        w = self._label(
            text="T: %.1f C",
            bind={"message": "m", "field": "t", "type": "float"},
        )
        self.assertTrue(w.text_is_format)

    def test_double_percent_only_is_a_template_but_needs_no_bind(self) -> None:
        w = self._label(text="100%%")
        self.assertTrue(w.text_is_format)  # runtime must collapse %% -> %

    def test_bare_percent_that_is_not_a_conversion_stays_literal(self) -> None:
        w = self._label(text="50% done")
        self.assertFalse(w.text_is_format)

    def test_plain_text_is_not_a_format(self) -> None:
        w = self._label(text="Hello")
        self.assertFalse(w.text_is_format)

    def test_no_text_is_not_a_format(self) -> None:
        w = self._label(bind={"message": "m", "field": "name", "type": "string"})
        self.assertFalse(w.text_is_format)


if __name__ == "__main__":
    unittest.main()
