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
RenderMode = Literal["blocking", "non_blocking"]
FontSize = Literal["medium", "large"]


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
    # Set by the Stage 1 parser, never authored. True when `text` is a
    # printf-style *template* rather than a literal string: it holds an
    # unescaped conversion (`%d %u %x %ld %lld %f %.Nf %s`) and/or a `%%`
    # escape. label/header only. Stage 3b bakes it as `.text_is_format`
    # and the runtime then formats `text` against this widget's `bind`
    # into a scratch buffer per draw instead of blitting it verbatim
    # (janus_format.c). A `text` with a real conversion also requires a
    # `bind` of a matching type — enforced in _validate_widget.
    text_is_format: bool = False
    asset: Optional[str] = None
    # kind == "image" only: filesystem path to a source image file (PNG,
    # BMP, JPEG, GIF, TIFF, WebP, ...), resolved to an absolute path
    # against the declaring .screen.yaml's own directory at parse time.
    # Stage 3b decodes it, composites any alpha over black, rescales to
    # this widget's `size`, and bakes an RGB565 pixel array into the
    # generated screen source. A missing / unsupported / undecodable file
    # is logged and the widget renders as a magenta placeholder rect —
    # never fatal to generation.
    image_file: Optional[str] = None
    value: Optional[Union[int, str]] = None
    range: Optional[tuple[float, float]] = None
    states: Optional[list[str]] = None
    size: Optional[tuple[int, int]] = None
    on_press: Optional[str] = None
    navigate: Optional[str] = None
    collapsible: bool = False
    default_expanded: bool = True
    layout: Optional[Literal["column", "row"]] = None
    # Stage 2: grow along the parent's main axis (column -> height, row ->
    # width) to consume an equal share of whatever's left over after
    # every sibling takes its normal size — see stage2_layout/layout.py.
    # False (default) keeps today's pure intrinsic-size behavior
    # unchanged; the cross axis is never affected by this.
    fill: bool = False
    # Static visibility (v1). `hidden: true` in the DSL makes Stage 1
    # drop this widget *and its whole subtree* from the parsed tree — no
    # geometry, no render, no harpia field, no baked image asset. It
    # exists only so two widgets (an enabled + a disabled icon, say) can
    # occupy one authored slot with exactly one kept. Because the prune
    # happens in the parser, nothing downstream ever sees a hidden node,
    # so this field is always False on any Widget that leaves Stage 1.
    # A field-bound `hidden` that toggles on-device is deliberately out
    # of scope here (that one needs a real baked rect + a runtime check).
    hidden: bool = False
    # "#RRGGBB" hex, packed to RGB565 at Stage 3b emission time — not
    # authored on-device, same "push work to build time" spirit as
    # geometry. `color` is the ink/foreground/on-state fill; `bg` is the
    # background/off-state fill. Both optional — Stage 3b falls back to a
    # fixed runtime default when either is omitted, so existing specs with
    # no color at all keep generating unchanged.
    color: Optional[str] = None
    bg: Optional[str] = None
    # Which runtime/embedded_c/include/janus_font.h table this widget's
    # text renders from ("large" == today's only size before this field
    # existed, so it's the default — see janus_font.h for why LARGE == 0
    # matters at the C struct level too), and an integer multiplier on top
    # of that table's native glyph size. Meaningless on a widget that never
    # draws text (unrestricted here, same as color/bg). `font_scale` is
    # capped in _validate_widget below at large's own 20x28 footprint —
    # see that function's comment for why.
    font_size: FontSize = "large"
    font_scale: int = 1
    children: list["Widget"] = field(default_factory=list)
    # box only: leaf widgets always rendered inside the header strip,
    # collapsed OR expanded — distinct from `children`, which only render
    # when expanded. View-only "at a glance" content (an icon, an LED, a
    # status label...); the header tap always toggles collapse regardless
    # of what's here, so on_press/navigate on a summary widget is never
    # actually dispatched. See stage2_layout/layout.py for how these are
    # positioned (right-aligned within the header row).
    summary: list["Widget"] = field(default_factory=list)
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
    # blocking (default) keeps today's behavior unchanged — main.c calls
    # janus_render_screen once and blocks until it's done. non_blocking
    # picks the janus_render_screen_async_start/janus_render_poll scaffold
    # instead (see Stage 8's scaffold_main.py) — declared, not baked into
    # every project unconditionally, same "input.modality" precedent.
    render_mode: RenderMode = "blocking"


@dataclass
class App:
    screens: list[Screen]
    nav: Optional[list[NavTarget]] = None
    display: Optional[DisplayConfig] = None
    # which physical input a generated project's main.c polls (Stage 6/8)
    # — one modality per project, default touch (today's only behavior).
    input_modality: InputModality = "touch"
