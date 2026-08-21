import unittest

from janus.stage3b_embedded_c.emit_files import (
    render_actions_header,
    render_app_source,
    render_bindings_header,
    render_bindings_source,
    render_display_config_header,
    render_screen_header,
    render_screen_source,
)
from janus.ir import App, Binding, DisplayConfig, NavTarget, Screen, Widget
from janus.stage2_layout.layout import layout_screen


def _balanced_braces(text: str) -> bool:
    depth = 0
    for ch in text:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


class TestEmitFiles(unittest.TestCase):
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

    def test_screen_header_has_guard_and_extern(self) -> None:
        out = render_screen_header(self.app.screens[0])
        self.assertIn("#ifndef JANUS_GEN_ONE_SCREEN_H", out)
        self.assertIn("#define JANUS_GEN_ONE_SCREEN_H", out)
        self.assertIn("#endif", out)
        self.assertIn("extern const janus_screen_desc_t one_screen;", out)
        self.assertIn('#include "janus_runtime.h"', out)
        self.assertTrue(_balanced_braces(out))

    def test_screen_source_includes_own_header_and_has_body(self) -> None:
        from janus.stage3b_embedded_c.emit_embedded_c import screen_index_map

        out = render_screen_source(self.app.screens[0], screen_index_map(self.app))
        self.assertIn('#include "one_screen.gen.h"', out)
        self.assertIn("const janus_screen_desc_t one_screen = {", out)
        self.assertIn(".navigate_target = 1", out)  # "Two" is index 1
        self.assertTrue(_balanced_braces(out))

    def test_screen_source_without_index_map_still_renders_unbound_navigation_error(self) -> None:
        with self.assertRaises(ValueError):
            render_screen_source(self.app.screens[0])

    def test_screen_source_includes_actions_header_when_screen_has_on_press(self) -> None:
        from janus.stage3b_embedded_c.emit_embedded_c import screen_index_map

        out = render_screen_source(self.app.screens[0], screen_index_map(self.app))
        self.assertIn('#include "janus_actions.gen.h"', out)

    def test_screen_source_omits_bindings_header_when_screen_has_no_binds(self) -> None:
        from janus.stage3b_embedded_c.emit_embedded_c import screen_index_map

        out = render_screen_source(self.app.screens[0], screen_index_map(self.app))
        self.assertNotIn('#include "janus_bindings.gen.h"', out)

    def test_screen_source_includes_bindings_header_when_screen_has_a_bind(self) -> None:
        from janus.ir import Binding

        bound_screen = Screen(
            name="Bound",
            root=Widget(kind="column", id="r3", children=[
                Widget(kind="label", id="lbl", bind=Binding(message="dev", field="name", type="string")),
            ]),
        )
        layout_screen(bound_screen)
        out = render_screen_source(bound_screen)
        self.assertIn('#include "janus_bindings.gen.h"', out)
        self.assertNotIn('#include "janus_actions.gen.h"', out)

    def test_actions_header_has_guard(self) -> None:
        out = render_actions_header(self.app)
        self.assertIn("#ifndef JANUS_GEN_ACTIONS_H", out)
        self.assertIn("#define JANUS_GEN_ACTIONS_H", out)
        self.assertIn("JANUS_ACTION_REBOOT", out)
        self.assertIn("#endif", out)
        self.assertTrue(_balanced_braces(out))

    def test_app_source_includes_runtime_and_has_body(self) -> None:
        out = render_app_source(self.app)
        self.assertIn('#include "janus_runtime.h"', out)
        self.assertIn("janus_app_t janus_app = {", out)
        self.assertIn('"Status"', out)
        self.assertTrue(_balanced_braces(out))

    def test_bindings_header_has_guard_and_struct(self) -> None:
        bound_screen = Screen(
            name="Bound",
            root=Widget(kind="column", id="r3", children=[
                Widget(kind="label", id="lbl", bind=Binding(message="dev", field="name", type="string")),
            ]),
        )
        layout_screen(bound_screen)
        out = render_bindings_header(App(screens=[bound_screen]))
        self.assertIn("#ifndef JANUS_GEN_BINDINGS_H", out)
        self.assertIn("#define JANUS_GEN_BINDINGS_H", out)
        self.assertIn("typedef struct {", out)
        self.assertIn("} dev_t;", out)
        self.assertIn("extern dev_t dev_instance;", out)
        self.assertIn("#endif", out)
        self.assertTrue(_balanced_braces(out))

    def test_bindings_source_includes_own_header_and_zero_inits(self) -> None:
        bound_screen = Screen(
            name="Bound",
            root=Widget(kind="column", id="r3", children=[
                Widget(kind="label", id="lbl", bind=Binding(message="dev", field="name", type="string")),
            ]),
        )
        layout_screen(bound_screen)
        out = render_bindings_source(App(screens=[bound_screen]))
        self.assertIn('#include "janus_bindings.gen.h"', out)
        self.assertIn("dev_t dev_instance = {0};", out)

    def test_display_config_header_has_guard_and_constants(self) -> None:
        out = render_display_config_header(DisplayConfig(width=240, height=320, color="rgb565"))
        self.assertIn("#ifndef JANUS_GEN_DISPLAY_CONFIG_H", out)
        self.assertIn("#define JANUS_GEN_DISPLAY_CONFIG_H", out)
        self.assertIn("#define JANUS_DISPLAY_WIDTH 240", out)
        self.assertIn("#define JANUS_DISPLAY_HEIGHT 320", out)
        self.assertIn("#define JANUS_DISPLAY_COLOR JANUS_DISPLAY_COLOR_RGB565", out)
        self.assertIn("#endif", out)
        self.assertTrue(_balanced_braces(out))

    def test_display_config_header_omits_bus_and_controller_selection_when_unset(self) -> None:
        out = render_display_config_header(DisplayConfig(width=240, height=320, color="mono"))
        self.assertIn("#define JANUS_DISPLAY_BUS_SPI 0", out)  # enumeration always present
        self.assertNotIn("#define JANUS_DISPLAY_BUS ", out)  # selection macro absent
        self.assertIn("#define JANUS_DISPLAY_CONTROLLER_ST7789 0", out)
        self.assertNotIn("#define JANUS_DISPLAY_CONTROLLER ", out)

    def test_display_config_header_selects_bus_and_controller_when_set(self) -> None:
        out = render_display_config_header(
            DisplayConfig(width=240, height=320, color="rgb565", bus="spi", controller="st7789v")
        )
        self.assertIn("#define JANUS_DISPLAY_BUS JANUS_DISPLAY_BUS_SPI", out)
        self.assertIn("#define JANUS_DISPLAY_CONTROLLER JANUS_DISPLAY_CONTROLLER_ST7789V", out)


if __name__ == "__main__":
    unittest.main()
