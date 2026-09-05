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


if __name__ == "__main__":
    unittest.main()
