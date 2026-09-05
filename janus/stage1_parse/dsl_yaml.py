"""Stage 1 — YAML parser. See architecture.md for the full contract.

`parse_screen`/`screen_from_dict` handle one `*.screen.yaml` file.
`parse_app`/`app_from_dict` handle `app.yaml`: resolving the screens it
references and running the one Stage 1 validation that needs
cross-screen knowledge (`button.navigate` naming a screen that actually
exists), deferred until now because it couldn't be enforced without
seeing every screen at once.
"""
from __future__ import annotations

import re
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
_VALID_RENDER_MODES = {"blocking", "non_blocking"}
_REQUIRES_RANGE = {"progress", "gauge", "slider"}
_CONTAINER_KINDS = {"column", "row", "box", "radiogroup", "navlist"}
_HEX_COLOR_RE = re.compile(r"^#[0-9A-Fa-f]{6}$")
# Native glyph dims per runtime/embedded_c/include/janus_font.h's two
# tables. `large` is also the cap `font_scale` can't push a widget's
# effective glyph size past — see _validate_widget's font_scale check for
# why (it mirrors janus_runtime.c's g_tile_buffer, sized for large's own
# 20x28 native footprint; scaling past that would need either a bigger
# static buffer or a tiled glyph blit, neither of which this runtime does).
_FONT_NATIVE_SIZE = {"medium": (10, 14), "large": (20, 28)}
_FONT_SCALE_CAP_W, _FONT_SCALE_CAP_H = _FONT_NATIVE_SIZE["large"]
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


def _parse_color(value: str | None) -> str | None:
    if value is None:
        return None
    if not _HEX_COLOR_RE.match(value):
        raise ValueError(f"invalid color {value!r} — must match #RRGGBB (6 hex digits)")
    return value


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
    if widget.font_size not in _FONT_NATIVE_SIZE:
        raise ValueError(
            f"widget {widget.id!r} has invalid font_size {widget.font_size!r} — "
            f"must be one of {sorted(_FONT_NATIVE_SIZE)}"
        )
    if not isinstance(widget.font_scale, int) or widget.font_scale < 1:
        raise ValueError(
            f"widget {widget.id!r}'s font_scale {widget.font_scale!r} must be a positive integer"
        )
    native_w, native_h = _FONT_NATIVE_SIZE[widget.font_size]
    scaled_w, scaled_h = native_w * widget.font_scale, native_h * widget.font_scale
    if scaled_w > _FONT_SCALE_CAP_W or scaled_h > _FONT_SCALE_CAP_H:
        raise ValueError(
            f"widget {widget.id!r}: font_size={widget.font_size!r} at font_scale="
            f"{widget.font_scale} needs a {scaled_w}x{scaled_h} glyph, past the "
            f"{_FONT_SCALE_CAP_W}x{_FONT_SCALE_CAP_H} cap (large's own native size — "
            f"the runtime's glyph buffer is sized for it, see janus_font.h)"
        )
    if widget.kind == "radiogroup" and widget.bind is not None:
        for child in widget.children:
            if child.kind == "radiobutton" and child.value is not None:
                _check_radiobutton_value(child, widget.bind.type)
    if widget.summary and widget.kind != "box":
        raise ValueError(
            f"widget {widget.id!r} (kind={widget.kind!r}) has `summary` — only `box` "
            f"widgets can have header-summary content"
        )
    for child in widget.summary:
        if child.kind in _CONTAINER_KINDS:
            raise ValueError(
                f"box {widget.id!r}'s summary widget {child.id!r} is a container "
                f"(kind={child.kind!r}) — summary only holds leaf widgets, no nesting"
            )


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
        fill=data.get("fill", False),
        color=_parse_color(data.get("color")),
        bg=_parse_color(data.get("bg")),
        font_size=data.get("font_size", "large"),
        font_scale=data.get("font_scale", 1),
        children=[_parse_widget(c) for c in data.get("children", [])],
        summary=[_parse_widget(c) for c in data.get("summary", [])],
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
    for child in widget.summary:
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
    render_mode = data.get("render_mode", "blocking")
    if render_mode not in _VALID_RENDER_MODES:
        raise ValueError(
            f"invalid display render_mode {render_mode!r} — must be one of "
            f"{sorted(_VALID_RENDER_MODES)}"
        )
    w, h = _parse_size(data["size"])
    return DisplayConfig(
        width=w, height=h, color=color, bus=bus, controller=controller, render_mode=render_mode
    )


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
