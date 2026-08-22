import unittest

from janus.stage3b_embedded_c.emit_embedded_c import (
    emit_actions_header,
    emit_app_table,
    emit_display_config,
    emit_screen,
    screen_index_map,
)
from janus.ir import App, DisplayConfig, NavTarget, Screen, Widget
from janus.stage2_layout.layout import layout_screen


class TestEmitDisplayConfigRenderMode(unittest.TestCase):
    def test_default_render_mode_emits_blocking_selection(self) -> None:
        out = emit_display_config(DisplayConfig(width=240, height=320, color="rgb565"))
        self.assertIn("#define JANUS_DISPLAY_RENDER_MODE JANUS_DISPLAY_RENDER_MODE_BLOCKING", out)

    def test_non_blocking_render_mode_emits_its_own_selection(self) -> None:
        out = emit_display_config(
            DisplayConfig(width=240, height=320, color="rgb565", render_mode="non_blocking")
        )
        self.assertIn("#define JANUS_DISPLAY_RENDER_MODE JANUS_DISPLAY_RENDER_MODE_NON_BLOCKING", out)

    def test_both_render_mode_values_are_always_defined(self) -> None:
        out = emit_display_config(DisplayConfig(width=240, height=320, color="rgb565"))
        self.assertIn("#define JANUS_DISPLAY_RENDER_MODE_BLOCKING", out)
        self.assertIn("#define JANUS_DISPLAY_RENDER_MODE_NON_BLOCKING", out)


class TestActionsAndAppTable(unittest.TestCase):
    def setUp(self) -> None:
        screen_one = Screen(
            name="One",
            root=Widget(kind="column", id="r1", children=[
                Widget(kind="button", id="reboot_button", text="Reboot", on_press="reboot"),
                Widget(kind="button", id="goto_two", text="Go", navigate="Two"),
            ]),
        )
        screen_two = Screen(
            name="Two",
            root=Widget(kind="column", id="r2", children=[
                Widget(kind="label", id="lbl", text="hi"),
            ]),
        )
        layout_screen(screen_one)
        layout_screen(screen_two)
        self.app = App(
            screens=[screen_one, screen_two],
            nav=[NavTarget(screen="One", title="Status"), NavTarget(screen="Two", title="Settings")],
        )

    def test_actions_header_has_on_press_but_not_navigate(self) -> None:
        out = emit_actions_header(self.app)
        self.assertIn("JANUS_ACTION_NONE", out)
        self.assertIn("JANUS_ACTION_REBOOT", out)
        self.assertNotIn("GOTO_TWO", out)  # navigate never becomes an action

    def test_actions_header_declares_janus_handle_action(self) -> None:
        # Stage 5's janus_handle_action() is defined in src/janus_actions.c
        # but never declared anywhere a caller could see it — Stage 6's
        # input dispatch needs to call it, so the generated header
        # declares it alongside the enum it depends on.
        out = emit_actions_header(self.app)
        self.assertIn("void janus_handle_action(janus_action_t action);", out)

    def test_navigate_target_resolves_to_screen_index(self) -> None:
        idx = screen_index_map(self.app)
        out = emit_screen(self.app.screens[0], screen_index_by_name=idx)
        self.assertIn(".action = JANUS_ACTION_REBOOT", out)
        self.assertIn(".action = JANUS_ACTION_NONE, .navigate_target = 1", out)  # "Two" is index 1

    def test_navigate_without_index_map_raises(self) -> None:
        with self.assertRaises(ValueError):
            emit_screen(self.app.screens[0])

    def test_app_table_lists_both_screens(self) -> None:
        out = emit_app_table(self.app)
        self.assertIn("extern const janus_screen_desc_t one_screen;", out)
        self.assertIn("extern const janus_screen_desc_t two_screen;", out)
        self.assertIn("&one_screen", out)
        self.assertIn("&two_screen", out)
        self.assertIn(".screen_count = 2,", out)

    def test_app_table_nav_titles_in_screen_order(self) -> None:
        out = emit_app_table(self.app)
        self.assertIn('"Status"', out)
        self.assertIn('"Settings"', out)
        self.assertIn(".nav_titles = janus_app_nav_titles,", out)

    def test_app_table_without_nav_has_null_titles(self) -> None:
        out = emit_app_table(App(screens=self.app.screens, nav=None))
        self.assertIn(".nav_titles = NULL,", out)

    def test_app_table_raises_if_nav_missing_a_screen(self) -> None:
        bad_app = App(screens=self.app.screens, nav=[NavTarget(screen="One", title="Status")])
        with self.assertRaises(ValueError):
            emit_app_table(bad_app)


if __name__ == "__main__":
    unittest.main()
