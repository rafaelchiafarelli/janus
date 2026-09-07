import unittest

from janus.stage1_parse.dsl_yaml import screen_from_dict


def _screen(children):
    return {"screen": "Validation", "layout": "column", "children": children}


class TestBindTypeValidation(unittest.TestCase):
    def test_invalid_bind_type_rejected(self) -> None:
        data = _screen([
            {"kind": "label", "id": "a", "bind": {"message": "m", "field": "f", "type": "bool"}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_valid_bind_types_accepted(self) -> None:
        for t in ("string", "int", "int64", "float"):
            data = _screen([
                {"kind": "label", "id": "a", "bind": {"message": "m", "field": "f", "type": t}},
            ])
            screen_from_dict(data)  # must not raise


class TestRangeValidation(unittest.TestCase):
    def test_progress_without_range_rejected(self) -> None:
        data = _screen([
            {"kind": "progress", "id": "p", "bind": {"message": "m", "field": "f", "type": "int"},
             "size": {"w": 10, "h": 10}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_gauge_without_range_rejected(self) -> None:
        data = _screen([
            {"kind": "gauge", "id": "g", "bind": {"message": "m", "field": "f", "type": "float"},
             "size": {"w": 10, "h": 10}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_progress_with_range_accepted(self) -> None:
        data = _screen([
            {"kind": "progress", "id": "p", "bind": {"message": "m", "field": "f", "type": "int"},
             "range": {"min": 0, "max": 100}, "size": {"w": 10, "h": 10}},
        ])
        screen_from_dict(data)  # must not raise

    def test_slider_without_range_rejected(self) -> None:
        data = _screen([
            {"kind": "slider", "id": "s", "bind": {"message": "m", "field": "f", "type": "int"},
             "size": {"w": 10, "h": 10}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_slider_with_range_accepted(self) -> None:
        data = _screen([
            {"kind": "slider", "id": "s", "bind": {"message": "m", "field": "f", "type": "int"},
             "range": {"min": 0, "max": 100}, "size": {"w": 10, "h": 10}},
        ])
        screen_from_dict(data)  # must not raise


class TestRadiobuttonValueValidation(unittest.TestCase):
    def test_mismatched_value_type_rejected(self) -> None:
        data = _screen([
            {"kind": "radiogroup", "id": "rg", "bind": {"message": "m", "field": "mode", "type": "int"},
             "layout": "row", "children": [
                 {"kind": "radiobutton", "id": "a", "value": "not_an_int", "text": "A"},
             ]},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_matching_value_type_accepted(self) -> None:
        data = _screen([
            {"kind": "radiogroup", "id": "rg", "bind": {"message": "m", "field": "mode", "type": "int"},
             "layout": "row", "children": [
                 {"kind": "radiobutton", "id": "a", "value": 0, "text": "A"},
                 {"kind": "radiobutton", "id": "b", "value": 1, "text": "B"},
             ]},
        ])
        screen_from_dict(data)  # must not raise


class TestColorValidation(unittest.TestCase):
    def test_malformed_hex_color_rejected(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "hi", "color": "red"}])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_short_hex_color_rejected(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "hi", "color": "#fff"}])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_valid_hex_color_and_bg_accepted(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "hi", "color": "#FF0000", "bg": "#00ff00"}])
        screen_from_dict(data)  # must not raise

    def test_color_omitted_is_fine(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "hi"}])
        screen_from_dict(data)  # must not raise


class TestBoxSummaryValidation(unittest.TestCase):
    def test_summary_on_non_box_rejected(self) -> None:
        data = _screen([
            {"kind": "column", "id": "c", "summary": [{"kind": "led", "id": "l", "size": {"w": 10, "h": 10}}]},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_container_kind_in_summary_rejected(self) -> None:
        data = _screen([
            {"kind": "box", "id": "b", "layout": "column",
             "summary": [{"kind": "row", "id": "r", "children": []}],
             "children": [{"kind": "label", "id": "d"}]},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_valid_summary_on_box_accepted(self) -> None:
        data = _screen([
            {"kind": "box", "id": "b", "layout": "column",
             "summary": [
                 {"kind": "led", "id": "l", "size": {"w": 10, "h": 10}},
                 {"kind": "label", "id": "m", "bind": {"message": "m", "field": "mode", "type": "string"}},
             ],
             "children": [{"kind": "label", "id": "d"}]},
        ])
        screen = screen_from_dict(data)  # must not raise
        box = screen.root.children[0]
        self.assertEqual(len(box.summary), 2)
        self.assertEqual(box.summary[0].kind, "led")

    def test_box_with_no_summary_defaults_to_empty(self) -> None:
        data = _screen([
            {"kind": "box", "id": "b", "layout": "column", "children": [{"kind": "label", "id": "d"}]},
        ])
        screen = screen_from_dict(data)
        self.assertEqual(screen.root.children[0].summary, [])


class TestFontSizeValidation(unittest.TestCase):
    def test_font_size_and_scale_omitted_default_to_large_and_1(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "hi"}])
        widget = screen_from_dict(data).root.children[0]
        self.assertEqual(widget.font_size, "large")
        self.assertEqual(widget.font_scale, 1)

    def test_invalid_font_size_rejected(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "hi", "font_size": "huge"}])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_valid_font_sizes_accepted(self) -> None:
        for size in ("medium", "large"):
            data = _screen([{"kind": "label", "id": "a", "text": "hi", "font_size": size}])
            widget = screen_from_dict(data).root.children[0]
            self.assertEqual(widget.font_size, size)

    def test_non_positive_font_scale_rejected(self) -> None:
        for scale in (0, -1):
            data = _screen([{"kind": "label", "id": "a", "text": "hi", "font_scale": scale}])
            with self.assertRaises(ValueError):
                screen_from_dict(data)

    def test_medium_font_scale_2_reaches_large_footprint_and_is_accepted(self) -> None:
        # 10x14 * 2 == large's own 20x28 native size — exactly at the cap.
        data = _screen([
            {"kind": "label", "id": "a", "text": "hi", "font_size": "medium", "font_scale": 2},
        ])
        widget = screen_from_dict(data).root.children[0]
        self.assertEqual(widget.font_scale, 2)

    def test_medium_font_scale_3_exceeds_large_footprint_and_is_rejected(self) -> None:
        data = _screen([
            {"kind": "label", "id": "a", "text": "hi", "font_size": "medium", "font_scale": 3},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_large_font_scale_2_exceeds_its_own_footprint_and_is_rejected(self) -> None:
        # large is already the cap -- it can't scale past its own native size.
        data = _screen([
            {"kind": "label", "id": "a", "text": "hi", "font_size": "large", "font_scale": 2},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)


class TestFormatTextValidation(unittest.TestCase):
    def test_conversion_without_bind_is_rejected(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "%d"}])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_double_percent_without_bind_is_fine(self) -> None:
        data = _screen([{"kind": "label", "id": "a", "text": "100%%"}])
        screen_from_dict(data)  # must not raise — no real conversion

    def test_string_conversion_needs_a_string_bind(self) -> None:
        data = _screen([
            {"kind": "label", "id": "a", "text": "%s",
             "bind": {"message": "m", "field": "n", "type": "int"}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_numeric_conversion_rejects_a_string_bind(self) -> None:
        data = _screen([
            {"kind": "label", "id": "a", "text": "%d",
             "bind": {"message": "m", "field": "n", "type": "string"}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_conversion_on_a_non_text_kind_is_rejected(self) -> None:
        data = _screen([
            {"kind": "button", "id": "a", "text": "go %d",
             "bind": {"message": "m", "field": "n", "type": "int"}},
        ])
        with self.assertRaises(ValueError):
            screen_from_dict(data)

    def test_valid_format_label_accepted(self) -> None:
        data = _screen([
            {"kind": "label", "id": "a", "text": "T: %.1f C",
             "bind": {"message": "m", "field": "t", "type": "float"}},
        ])
        w = screen_from_dict(data).root.children[0]
        self.assertTrue(w.text_is_format)

    def test_second_conversion_warns_but_does_not_raise(self) -> None:
        data = _screen([
            {"kind": "label", "id": "a", "text": "%d of %d",
             "bind": {"message": "m", "field": "n", "type": "int"}},
        ])
        with self.assertLogs("janus.parse", level="WARNING"):
            screen_from_dict(data)  # must not raise


if __name__ == "__main__":
    unittest.main()
