"""IR data contract — see architecture.md. Shared by every pipeline stage
after the YAML parser; nothing downstream touches YAML directly.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal, Optional, Union

BindType = Literal["string", "int", "int64", "float"]
DisplayColor = Literal["mono", "gray", "rgb565"]
DisplayBus = Literal["spi", "i2c", "parallel"]
DisplayController = Literal[
    "st7789", "st7789v", "ili9341", "ili9341v", "hx8357",
    "gc9a01", "ssd1306", "sh1106", "il3820", "il0373",
]
InputModality = Literal["touch", "encoder", "buttons"]


@dataclass
class Rect:
    x: int
    y: int
    w: int
    h: int


@dataclass
class Binding:
    message: str
    field: str
    type: BindType


@dataclass
class Widget:
    kind: str
    id: str
    bind: Optional[Binding] = None
    text: Optional[str] = None
    asset: Optional[str] = None
    value: Optional[Union[int, str]] = None
    range: Optional[tuple[float, float]] = None
    states: Optional[list[str]] = None
    size: Optional[tuple[int, int]] = None
    on_press: Optional[str] = None
    navigate: Optional[str] = None
    collapsible: bool = False
    default_expanded: bool = True
    layout: Optional[Literal["column", "row"]] = None
    children: list["Widget"] = field(default_factory=list)
    # filled in later by the layout pass; always empty coming out of the parser
    geometry: Optional[Rect] = None
    geometry_collapsed: Optional[Rect] = None


@dataclass
class Screen:
    name: str
    root: Widget


@dataclass
class NavTarget:
    screen: str
    title: str


@dataclass
class DisplayConfig:
    width: int
    height: int
    color: DisplayColor
    # bus/controller are a hardware *selection*, not a driver — Janus
    # emits them as data (see emit_display_config), the same seam
    # size/color already use. The actual driver body stays human-owned
    # (settled 2026-08-20, see Janus.md's Open Questions) — keeping the
    # selection as plain data here is what makes swapping in a
    # Janus-provided driver library later "boltable" rather than a
    # schema change: a future library would switch on this same value.
    bus: Optional[DisplayBus] = None
    controller: Optional[DisplayController] = None


@dataclass
class App:
    screens: list[Screen]
    nav: Optional[list[NavTarget]] = None
    display: Optional[DisplayConfig] = None
    # which physical input a generated project's main.c polls (Stage 6/8)
    # — one modality per project, default touch (today's only behavior).
    input_modality: InputModality = "touch"
