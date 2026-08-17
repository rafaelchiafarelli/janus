"""Stage 1 — YAML parser. See architecture.md for the full contract.

Scope right now: single-screen files only (`parse_screen`). `app.yaml`
(multi-screen + nav) parsing — and with it, the one Stage 1 validation
that needs cross-screen knowledge (`button.navigate` naming a screen that
actually exists) — is a separate, later increment.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

from .ir import Binding, Screen, Widget

_VALID_BIND_TYPES = {"string", "int", "int64", "float"}
_REQUIRES_RANGE = {"progress", "gauge"}
_PY_TYPE_FOR_BIND_TYPE: dict[str, type | tuple[type, ...]] = {
    "string": str,
    "int": int,
    "int64": int,
    "float": (int, float),
}


def _parse_binding(data: dict[str, Any] | None) -> Binding | None:
    if data is None:
        return None
    bind_type = data["type"]
    if bind_type not in _VALID_BIND_TYPES:
        raise ValueError(
            f"invalid bind type {bind_type!r} — must be one of {sorted(_VALID_BIND_TYPES)}"
        )
    return Binding(message=data["message"], field=data["field"], type=bind_type)


def _parse_size(data: dict[str, Any] | None) -> tuple[int, int] | None:
    if data is None:
        return None
    return (data["w"], data["h"])


def _parse_range(data: dict[str, Any] | None) -> tuple[float, float] | None:
    if data is None:
        return None
    return (data["min"], data["max"])


def _check_radiobutton_value(radiobutton: Widget, bind_type: str) -> None:
    expected = _PY_TYPE_FOR_BIND_TYPE[bind_type]
    if not isinstance(radiobutton.value, expected):
        raise ValueError(
            f"radiobutton {radiobutton.id!r} value {radiobutton.value!r} doesn't match "
            f"its radiogroup's bind type {bind_type!r}"
        )


def _validate_widget(widget: Widget) -> None:
    if widget.kind in _REQUIRES_RANGE and widget.range is None:
        raise ValueError(f"widget {widget.id!r} (kind={widget.kind!r}) requires `range`")
    if widget.kind == "radiogroup" and widget.bind is not None:
        for child in widget.children:
            if child.kind == "radiobutton" and child.value is not None:
                _check_radiobutton_value(child, widget.bind.type)


def _parse_widget(data: dict[str, Any]) -> Widget:
    widget = Widget(
        kind=data["kind"],
        id=data.get("id", ""),
        bind=_parse_binding(data.get("bind")),
        text=data.get("text"),
        asset=data.get("asset"),
        value=data.get("value"),
        range=_parse_range(data.get("range")),
        states=data.get("states"),
        size=_parse_size(data.get("size")),
        on_press=data.get("on_press"),
        navigate=data.get("navigate"),
        collapsible=data.get("collapsible", False),
        default_expanded=data.get("default_expanded", True),
        layout=data.get("layout"),
        children=[_parse_widget(c) for c in data.get("children", [])],
    )
    _validate_widget(widget)
    return widget


def screen_from_dict(data: dict[str, Any]) -> Screen:
    root = Widget(
        kind=data["layout"],
        id=f"{data['screen']}__root",
        layout=data["layout"],
        children=[_parse_widget(c) for c in data.get("children", [])],
    )
    return Screen(name=data["screen"], root=root)


def parse_screen(path: str | Path) -> Screen:
    data = yaml.safe_load(Path(path).read_text())
    return screen_from_dict(data)
