"""Stage 1 — YAML parser. See architecture.md for the full contract.

`parse_screen`/`screen_from_dict` handle one `*.screen.yaml` file.
`parse_app`/`app_from_dict` handle `app.yaml`: resolving the screens it
references and running the one Stage 1 validation that needs
cross-screen knowledge (`button.navigate` naming a screen that actually
exists), deferred until now because it couldn't be enforced without
seeing every screen at once.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

from ..ir import App, Binding, DisplayConfig, NavTarget, Screen, Widget

_VALID_BIND_TYPES = {"string", "int", "int64", "float"}
_VALID_DISPLAY_COLORS = {"mono", "gray", "rgb565"}
_VALID_DISPLAY_BUSES = {"spi", "i2c", "parallel"}
_VALID_DISPLAY_CONTROLLERS = {
    "st7789", "st7789v", "ili9341", "ili9341v", "hx8357",
    "gc9a01", "ssd1306", "sh1106", "il3820", "il0373",
}
_VALID_INPUT_MODALITIES = {"touch", "encoder", "buttons"}
_REQUIRES_RANGE = {"progress", "gauge", "slider"}
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


def _check_navigate_targets(widget: Widget, screen_names: set[str]) -> None:
    if widget.navigate is not None and widget.navigate not in screen_names:
        raise ValueError(
            f"button {widget.id!r} navigates to {widget.navigate!r}, which isn't "
            f"one of the screens listed in app.yaml ({sorted(screen_names)})"
        )
    for child in widget.children:
        _check_navigate_targets(child, screen_names)


def _parse_display(data: dict[str, Any] | None) -> DisplayConfig | None:
    if data is None:
        return None
    color = data.get("color", "mono")
    if color not in _VALID_DISPLAY_COLORS:
        raise ValueError(
            f"invalid display color {color!r} — must be one of {sorted(_VALID_DISPLAY_COLORS)}"
        )
    bus = data.get("bus")
    if bus is not None and bus not in _VALID_DISPLAY_BUSES:
        raise ValueError(
            f"invalid display bus {bus!r} — must be one of {sorted(_VALID_DISPLAY_BUSES)}"
        )
    controller = data.get("controller")
    if controller is not None and controller not in _VALID_DISPLAY_CONTROLLERS:
        raise ValueError(
            f"invalid display controller {controller!r} — must be one of "
            f"{sorted(_VALID_DISPLAY_CONTROLLERS)}"
        )
    w, h = _parse_size(data["size"])
    return DisplayConfig(width=w, height=h, color=color, bus=bus, controller=controller)


def _parse_input_modality(data: dict[str, Any] | None) -> str:
    if data is None:
        return "touch"
    modality = data.get("modality", "touch")
    if modality not in _VALID_INPUT_MODALITIES:
        raise ValueError(
            f"invalid input modality {modality!r} — must be one of {sorted(_VALID_INPUT_MODALITIES)}"
        )
    return modality


def app_from_dict(data: dict[str, Any], screens: list[Screen]) -> App:
    """`screens` are already-parsed `Screen` objects, in `app.yaml`'s
    `screens:` order — `parse_app` is what actually reads each file."""
    nav_data = data.get("nav")
    nav = None
    if nav_data is not None:
        nav = [
            NavTarget(screen=t["screen"], title=t["title"]) for t in nav_data["targets"]
        ]

    screen_names = {s.name for s in screens}
    for screen in screens:
        _check_navigate_targets(screen.root, screen_names)

    return App(
        screens=screens,
        nav=nav,
        display=_parse_display(data.get("display")),
        input_modality=_parse_input_modality(data.get("input")),
    )


def parse_app(path: str | Path) -> App:
    path = Path(path)
    data = yaml.safe_load(path.read_text())
    screens = [parse_screen(path.parent / screen_path) for screen_path in data["screens"]]
    return app_from_dict(data, screens)
