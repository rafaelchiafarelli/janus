import unittest
from pathlib import Path

from janus.dsl_yaml import app_from_dict, parse_app
from janus.ir import NavTarget, Screen, Widget

FIXTURES = Path(__file__).parent / "fixtures"


class TestParseApp(unittest.TestCase):
    def test_reads_screens_in_order_and_nav(self) -> None:
        app = parse_app(FIXTURES / "app.yaml")

        self.assertEqual([s.name for s in app.screens], ["UserProfile", "BoxDemo"])
        self.assertEqual(
            app.nav,
            [
                NavTarget(screen="UserProfile", title="Profile"),
                NavTarget(screen="BoxDemo", title="Boxes"),
            ],
        )

    def test_screens_are_fully_parsed_not_stubs(self) -> None:
        app = parse_app(FIXTURES / "app.yaml")
        user_profile = app.screens[0]
        self.assertEqual(user_profile.root.layout, "column")
        self.assertEqual(len(user_profile.root.children), 2)


class TestAppFromDictNavValidation(unittest.TestCase):
    def _screens(self, navigate: str | None) -> list[Screen]:
        one = Screen(
            name="One",
            root=Widget(kind="column", id="r1", children=[
                Widget(kind="button", id="go", text="Go", navigate=navigate),
            ]),
        )
        two = Screen(name="Two", root=Widget(kind="column", id="r2", children=[]))
        return [one, two]

    def test_navigate_to_existing_screen_accepted(self) -> None:
        app_from_dict({"screens": []}, self._screens("Two"))  # must not raise

    def test_navigate_to_missing_screen_rejected(self) -> None:
        with self.assertRaises(ValueError):
            app_from_dict({"screens": []}, self._screens("NoSuchScreen"))

    def test_no_navigate_is_fine(self) -> None:
        app_from_dict({"screens": []}, self._screens(None))  # must not raise

    def test_no_nav_key_means_no_tabs(self) -> None:
        app = app_from_dict({"screens": []}, self._screens(None))
        self.assertIsNone(app.nav)


if __name__ == "__main__":
    unittest.main()
