import unittest

from janus.dsl_yaml import screen_from_dict


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


if __name__ == "__main__":
    unittest.main()
