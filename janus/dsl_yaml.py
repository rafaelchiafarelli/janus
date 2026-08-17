"""Stage 1 — YAML parser. See architecture.md for the full contract.

Scope right now: single-screen files only (`parse_screen`). `app.yaml`
(multi-screen + nav) parsing is a separate, later increment.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

from .ir import Binding, Screen, Widget


def _parse_binding(data: dict[str, Any] | None) -> Binding | None:
    if data is None:
        return None
    return Binding(message=data["message"], field=data["field"], type=data["type"])


def _parse_size(data: dict[str, Any] | None) -> tuple[int, int] | None:
    if data is None:
        return None
    return (data["w"], data["h"])


def _parse_range(data: dict[str, Any] | None) -> tuple[float, float] | None:
    if data is None:
        return None
    return (data["min"], data["max"])


def _parse_widget(data: dict[str, Any]) -> Widget:
    return Widget(
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


def parse_screen(path: str | Path) -> Screen:
    data = yaml.safe_load(Path(path).read_text())
    root = Widget(
        kind=data["layout"],
        id=f"{data['screen']}__root",
        layout=data["layout"],
        children=[_parse_widget(c) for c in data.get("children", [])],
    )
    return Screen(name=data["screen"], root=root)
