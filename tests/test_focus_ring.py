"""`focus_ring: true` rows (fixes/000005): Stage 1 validation, Stage 2's
ring-width inset, and Stage 3b's per-screen focus-ring table + slots."""
import re
import unittest
from pathlib import Path

from janus.ir import DisplayConfig
from janus.stage1_parse.dsl_yaml import screen_from_dict
from janus.stage2_layout.layout import FOCUS_RING_PAD, GAP, layout_screen
from janus.stage3b_embedded_c.emit_embedded_c import emit_screen

REPO = Path(__file__).resolve().parent.parent


def _screen(children):
    return {"screen": "Rings", "layout": "column", "children": children}


def _relay_row(n: int, **extra):
    return {
        "kind": "row", "id": f"row_{n}", **extra,
        "children": [
            {"kind": "toggle", "id": f"relay_{n}", "on_press": f"toggle_relay_{n}"},
            {"kind": "label", "id": f"relay_{n}_label", "text": f"Relay {n}"},
        ],
    }


def _widget_line(out: str, widget_id: str) -> str:
    line = next(l for l in out.splitlines() if f'"{widget_id}"' in l and "static const char" in l)
    var = line.split()[3].split("[")[0]
    return next(l for l in out.splitlines() if f".id = {var}," in l)


class TestFocusRingValidation(unittest.TestCase):
    def test_ring_row_parses(self) -> None:
        screen = screen_from_dict(_screen([_relay_row(0, focus_ring=True)]))
        self.assertTrue(screen.root.children[0].focus_ring)

    def test_default_is_off(self) -> None:
        screen = screen_from_dict(_screen([_relay_row(0)]))
        self.assertFalse(screen.root.children[0].focus_ring)

    def test_only_rows(self) -> None:
        data = _screen([{"kind": "column", "id": "c", "focus_ring": True, "children": [
            {"kind": "button", "id": "b", "text": "Go", "on_press": "go"}]}])
        with self.assertRaisesRegex(ValueError, "only `row`"):
            screen_from_dict(data)

    def test_must_be_bool(self) -> None:
        with self.assertRaisesRegex(ValueError, "true or false"):
            screen_from_dict(_screen([_relay_row(0, focus_ring="yes")]))

    def test_row_with_nothing_focusable_rejected(self) -> None:
        data = _screen([{"kind": "row", "id": "r", "focus_ring": True, "children": [
            {"kind": "label", "id": "l", "text": "static"}]}])
        with self.assertRaisesRegex(ValueError, "nothing inside it is focusable"):
            screen_from_dict(data)

    def test_nested_ring_rows_rejected(self) -> None:
        data = _screen([{"kind": "row", "id": "outer", "focus_ring": True, "children": [
            {"kind": "column", "id": "c", "children": [_relay_row(0, focus_ring=True)]}]}])
        with self.assertRaisesRegex(ValueError, "can't nest"):
            screen_from_dict(data)


class TestFocusRingLayout(unittest.TestCase):
    def test_pad_matches_the_runtime_ring_width(self) -> None:
        src = (REPO / "runtime/embedded_c/src/janus_runtime.c").read_text()
        m = re.search(r"#define JANUS_FOCUS_RING_W (\d+)", src)
        self.assertIsNotNone(m)
        self.assertEqual(int(m.group(1)), FOCUS_RING_PAD)

    def test_ring_row_insets_its_children(self) -> None:
        plain = layout_screen(screen_from_dict(_screen([_relay_row(0)])))
        ringed = layout_screen(screen_from_dict(_screen([_relay_row(0, focus_ring=True)])))
        p_row, r_row = plain.root.children[0], ringed.root.children[0]
        p = FOCUS_RING_PAD
        self.assertEqual(r_row.geometry.w, p_row.geometry.w + 2 * p)
        self.assertEqual(r_row.geometry.h, p_row.geometry.h + 2 * p)
        for pc, rc in zip(p_row.children, r_row.children):
            self.assertEqual((rc.geometry.x, rc.geometry.y), (pc.geometry.x + p, pc.geometry.y + p))
            self.assertEqual((rc.geometry.w, rc.geometry.h), (pc.geometry.w, pc.geometry.h))

    def test_fill_child_shares_the_width_left_inside_the_inset(self) -> None:
        row = _relay_row(0, focus_ring=True)
        row["children"][1]["fill"] = True
        data = _screen([row])
        screen = layout_screen(screen_from_dict(data), DisplayConfig(width=200, height=100, color="rgb565"))
        r = screen.root.children[0]
        toggle, label = r.children
        self.assertEqual(r.geometry.w, 200)
        self.assertEqual(label.geometry.x + label.geometry.w, 200 - FOCUS_RING_PAD)
        self.assertEqual(label.geometry.x, toggle.geometry.x + toggle.geometry.w + GAP)


class TestFocusRingEmit(unittest.TestCase):
    def test_table_and_slots(self) -> None:
        screen = layout_screen(screen_from_dict(_screen([
            _relay_row(0, focus_ring=True),
            _relay_row(1, focus_ring=True, bg="#000000"),
            {"kind": "button", "id": "free", "text": "Go", "on_press": "go"},
        ])))
        out = emit_screen(screen)
        r0, r1 = (screen.root.children[i].geometry for i in (0, 1))
        self.assertIn(
            f"static const janus_focus_ring_t rings_focus_rings[] JANUS_PROGMEM = {{\n"
            f"    {{ .rect = {{{r0.x}, {r0.y}, {r0.w}, {r0.h}}}, .bg_color = JANUS_COLOR_DEFAULT_BG }},\n"
            f"    {{ .rect = {{{r1.x}, {r1.y}, {r1.w}, {r1.h}}}, .bg_color = 0x0000 }}\n}};",
            out,
        )
        self.assertIn(".focus_rings = rings_focus_rings,", out)
        self.assertIn(".focus_ring = 1,", _widget_line(out, "relay_0"))
        self.assertIn(".focus_ring = 2,", _widget_line(out, "relay_1"))
        # non-focusable siblings and widgets outside ring rows carry no slot
        self.assertNotIn(".focus_ring", _widget_line(out, "relay_0_label"))
        self.assertNotIn(".focus_ring", _widget_line(out, "free"))

    def test_screen_without_ring_rows_emits_nothing_new(self) -> None:
        out = emit_screen(layout_screen(screen_from_dict(_screen([_relay_row(0)]))))
        self.assertNotIn("focus_ring", out)


if __name__ == "__main__":
    unittest.main()
