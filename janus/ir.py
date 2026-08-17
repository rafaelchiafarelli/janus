"""IR data contract — see architecture.md. Shared by every pipeline stage
after the YAML parser; nothing downstream touches YAML directly.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal, Optional, Union

BindType = Literal["string", "int", "int64", "float"]


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
class App:
    screens: list[Screen]
    nav: Optional[list[NavTarget]] = None
