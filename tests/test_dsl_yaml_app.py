import unittest
from pathlib import Path

from janus.stage1_parse.dsl_yaml import app_from_dict, parse_app
from janus.ir import DisplayConfig, NavTarget, Screen, Widget

FIXTURES = Path(__file__).parent / "fixtures"


class TestParseApp(unittest.TestCase):
    def test_reads_screens_in_order(self) -> None:
        app = parse_app(FIXTURES / "app.yaml")
        self.assertEqual([s.name for s in app.screens], ["UserProfile", "BoxDemo"])
        self.assertIsNone(app.nav)  # the bare fixture declares no nav

    def test_reads_nav_targets_in_order(self) -> None:
        # nav requires a display (decision 1) — app_with_display.yaml has both
        app = parse_app(FIXTURES / "app_with_display.yaml")
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

    def _nav_data(self, **extra):
        return {
            "screens": [],
            "nav": {"kind": "tabs", "targets": [
                {"screen": "One", "title": "1"}, {"screen": "Two", "title": "2"},
            ]},
            **extra,
        }

    def test_nav_with_display_accepted(self) -> None:
        data = self._nav_data(display={"size": {"w": 240, "h": 320}})
        app = app_from_dict(data, self._screens(None))
        self.assertEqual([t.title for t in app.nav], ["1", "2"])

    def test_nav_without_display_rejected(self) -> None:
        with self.assertRaises(ValueError):
            app_from_dict(self._nav_data(), self._screens(None))

    def test_nav_target_not_a_screen_rejected(self) -> None:
        data = {
            "screens": [],
            "display": {"size": {"w": 240, "h": 320}},
            "nav": {"kind": "tabs", "targets": [{"screen": "Ghost", "title": "g"}]},
        }
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens(None))

    def test_too_many_tabs_for_panel_width_rejected(self) -> None:
        # 12 tabs on a 240px panel -> 20px each, under the 24px minimum
        data = {
            "screens": [],
            "display": {"size": {"w": 240, "h": 320}},
            "nav": {"kind": "tabs", "targets": [
                {"screen": "One", "title": str(i)} for i in range(12)
            ]},
        }
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens(None))


class TestAppFromDictDisplay(unittest.TestCase):
    def _screens(self) -> list[Screen]:
        return [Screen(name="One", root=Widget(kind="column", id="r1", children=[]))]

    def test_no_display_key_means_no_display_config(self) -> None:
        app = app_from_dict({"screens": []}, self._screens())
        self.assertIsNone(app.display)

    def test_display_parsed_with_default_color(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}}}
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.display, DisplayConfig(width=240, height=320, color="mono"))

    def test_display_parsed_with_explicit_color(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}, "color": "rgb565"}}
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.display, DisplayConfig(width=240, height=320, color="rgb565"))

    def test_invalid_display_color_rejected(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}, "color": "rgba"}}
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())

    def test_bus_and_controller_default_to_none(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}}}
        app = app_from_dict(data, self._screens())
        self.assertIsNone(app.display.bus)
        self.assertIsNone(app.display.controller)

    def test_bus_and_controller_parsed_when_given(self) -> None:
        data = {
            "screens": [],
            "display": {
                "size": {"w": 240, "h": 320},
                "bus": "spi",
                "controller": "st7789v",
            },
        }
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.display.bus, "spi")
        self.assertEqual(app.display.controller, "st7789v")

    def test_invalid_bus_rejected(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}, "bus": "usb"}}
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())

    def test_invalid_controller_rejected(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}, "controller": "st7735"}}
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())

    def test_render_mode_defaults_to_blocking(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}}}
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.display.render_mode, "blocking")

    def test_render_mode_parsed_when_given(self) -> None:
        data = {
            "screens": [],
            "display": {"size": {"w": 240, "h": 320}, "render_mode": "non_blocking"},
        }
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.display.render_mode, "non_blocking")

    def test_invalid_render_mode_rejected(self) -> None:
        data = {
            "screens": [],
            "display": {"size": {"w": 240, "h": 320}, "render_mode": "polled"},
        }
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())

    def test_background_defaults_to_none(self) -> None:
        data = {"screens": [], "display": {"size": {"w": 240, "h": 320}}}
        app = app_from_dict(data, self._screens())
        self.assertIsNone(app.display.background)

    def test_background_parsed_when_given(self) -> None:
        data = {
            "screens": [],
            "display": {"size": {"w": 240, "h": 320}, "background": "#001122"},
        }
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.display.background, "#001122")

    def test_invalid_background_rejected(self) -> None:
        data = {
            "screens": [],
            "display": {"size": {"w": 240, "h": 320}, "background": "black"},
        }
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())


class TestAppFromDictStatusValidation(unittest.TestCase):
    def _screens(self) -> list[Screen]:
        return [Screen(name="One", root=Widget(kind="column", id="r1", children=[]))]

    def test_no_status_key_means_none(self) -> None:
        app = app_from_dict({"screens": []}, self._screens())
        self.assertIsNone(app.status)

    def test_status_with_display_accepted(self) -> None:
        data = {
            "screens": [],
            "status": {"text": "Status: OK"},
            "display": {"size": {"w": 240, "h": 320}},
        }
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.status.text, "Status: OK")

    def test_status_without_display_rejected(self) -> None:
        data = {"screens": [], "status": {"text": "Status: OK"}}
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())

    def test_status_and_nav_can_coexist(self) -> None:
        data = {
            "screens": [],
            "status": {"text": "Status: OK"},
            "nav": {"kind": "tabs", "targets": [{"screen": "One", "title": "1"}]},
            "display": {"size": {"w": 240, "h": 320}},
        }
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.status.text, "Status: OK")
        self.assertEqual(app.nav, [NavTarget(screen="One", title="1")])


class TestAppFromDictInput(unittest.TestCase):
    def _screens(self) -> list[Screen]:
        return [Screen(name="One", root=Widget(kind="column", id="r1", children=[]))]

    def test_no_input_key_defaults_to_touch(self) -> None:
        app = app_from_dict({"screens": []}, self._screens())
        self.assertEqual(app.input_modality, "touch")

    def test_modality_parsed_when_given(self) -> None:
        data = {"screens": [], "input": {"modality": "encoder"}}
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.input_modality, "encoder")

    def test_buttons_modality_accepted(self) -> None:
        data = {"screens": [], "input": {"modality": "buttons"}}
        app = app_from_dict(data, self._screens())
        self.assertEqual(app.input_modality, "buttons")

    def test_invalid_modality_rejected(self) -> None:
        data = {"screens": [], "input": {"modality": "joystick"}}
        with self.assertRaises(ValueError):
            app_from_dict(data, self._screens())


if __name__ == "__main__":
    unittest.main()
