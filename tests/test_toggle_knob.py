"""toggle's authored knob colours (`knob_off` / `knob_on`, fixes/000007)."""
import unittest

from janus.stage1_parse.dsl_yaml import screen_from_dict
from janus.stage2_layout.layout import layout_screen
from janus.stage3b_embedded_c.emit_embedded_c import emit_screen


def _screen(toggle_extra, kind="toggle"):
    return {"screen": "Knob", "layout": "column", "children": [
        {"kind": kind, "id": "sw", "bind": {"message": "m", "field": "f", "type": "int"},
         **toggle_extra},
    ]}


def _widget_line(out: str, widget_id: str) -> str:
    line = next(l for l in out.splitlines() if f'"{widget_id}"' in l and "static const char" in l)
    var = line.split()[3].split("[")[0]
    return next(l for l in out.splitlines() if f".id = {var}," in l)


class TestToggleKnobParse(unittest.TestCase):
    def test_parses_both(self) -> None:
        w = screen_from_dict(_screen({"knob_off": "#FF0000", "knob_on": "#00FF00"})).root.children[0]
        self.assertEqual((w.knob_off, w.knob_on), ("#FF0000", "#00FF00"))

    def test_default_unset(self) -> None:
        w = screen_from_dict(_screen({})).root.children[0]
        self.assertEqual((w.knob_off, w.knob_on), (None, None))

    def test_invalid_colour_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "#RRGGBB"):
            screen_from_dict(_screen({"knob_on": "green"}))

    def test_only_toggles(self) -> None:
        with self.assertRaisesRegex(ValueError, "only `toggle`"):
            screen_from_dict(_screen({"knob_on": "#00FF00"}, kind="checkbox"))


class TestToggleKnobEmit(unittest.TestCase):
    def _emit(self, extra):
        return _widget_line(emit_screen(layout_screen(screen_from_dict(_screen(extra)))), "sw")

    def test_both_set(self) -> None:
        line = self._emit({"bg": "#D0D0D0", "color": "#D0D0D0",
                           "knob_off": "#FF0000", "knob_on": "#00FF00"})
        self.assertIn(".knob_off_color = 0xf800, .knob_on_color = 0x07e0, "
                      ".knob_flags = JANUS_KNOB_OFF_SET | JANUS_KNOB_ON_SET,", line)

    def test_one_set(self) -> None:
        line = self._emit({"knob_on": "#00FF00"})
        self.assertIn(".knob_on_color = 0x07e0, .knob_flags = JANUS_KNOB_ON_SET,", line)
        self.assertNotIn("knob_off_color", line)

    def test_unset_emits_nothing_new(self) -> None:
        line = self._emit({})
        self.assertNotIn("knob_off_color", line)
        self.assertNotIn("knob_on_color", line)
        self.assertNotIn("knob_flags", line)


if __name__ == "__main__":
    unittest.main()
