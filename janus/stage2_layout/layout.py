"""Stage 2 — layout pass. See architecture.md for the full contract.

Pure, deterministic: walks a Screen's widget tree bottom-up, filling in
`geometry` (and `geometry_collapsed` for `box`) on every widget. No I/O.
"""
from __future__ import annotations

from ..ir import DisplayConfig, Rect, Screen, Widget

GAP = 4
BOX_HEADER_H = 16

_LEAF_KINDS = {
    "label", "header", "button", "image", "progress", "gauge",
    "checkbox", "radiobutton", "led",
    "divider", "toggle", "badge", "slider",
}
_REQUIRES_EXPLICIT_SIZE = {"progress", "gauge", "image", "led", "badge", "slider"}
_DEFAULT_SIZE = {
    "label": (60, 12),
    "header": (80, 16),
    "button": (64, 20),
    "checkbox": (12, 12),
    "radiobutton": (12, 12),
    "divider": (60, 2),
    "toggle": (24, 12),
}


def layout_screen(screen: Screen) -> Screen:
    _layout_widget(screen.root, x=0, y=0)
    return screen


def check_fits_display(screen: Screen, display: DisplayConfig) -> None:
    """Call after `layout_screen`. `app.display` is optional (see Stage
    1) — this check only runs where a project has actually declared a
    real panel size to validate against; there's nothing to check
    otherwise. Raises rather than silently clipping — matches the
    parse-time "validate, don't default" rule this pipeline uses
    elsewhere (architecture.md Stage 1)."""
    root = screen.root.geometry
    if root.w > display.width or root.h > display.height:
        raise ValueError(
            f"screen {screen.name!r} needs {root.w}x{root.h}, which doesn't fit "
            f"the declared display size {display.width}x{display.height}"
        )


def _resolve_leaf_size(widget: Widget) -> tuple[int, int]:
    if widget.size is not None:
        return widget.size
    if widget.kind in _REQUIRES_EXPLICIT_SIZE:
        raise ValueError(
            f"widget {widget.id!r} (kind={widget.kind!r}) requires an "
            f"explicit `size` — its dimensions aren't derivable"
        )
    return _DEFAULT_SIZE[widget.kind]


def _direction_of(widget: Widget) -> str:
    if widget.layout in ("column", "row"):
        return widget.layout
    if widget.kind in ("column", "row"):
        return widget.kind
    return "column"


def _layout_widget(widget: Widget, x: int, y: int) -> None:
    if widget.kind in _LEAF_KINDS:
        w, h = _resolve_leaf_size(widget)
        widget.geometry = Rect(x=x, y=y, w=w, h=h)
        return

    direction = _direction_of(widget)
    header_h = BOX_HEADER_H if widget.kind == "box" else 0

    cursor_x, cursor_y = x, y + header_h
    for i, child in enumerate(widget.children):
        if i > 0:
            if direction == "column":
                cursor_y += GAP
            else:
                cursor_x += GAP
        _layout_widget(child, cursor_x, cursor_y)
        if direction == "column":
            cursor_y += child.geometry.h
        else:
            cursor_x += child.geometry.w

    if direction == "column":
        body_w = max((c.geometry.w for c in widget.children), default=0)
        body_h = cursor_y - (y + header_h)
    else:
        body_w = cursor_x - x
        body_h = max((c.geometry.h for c in widget.children), default=0)

    widget.geometry = Rect(x=x, y=y, w=body_w, h=header_h + body_h)
    if widget.kind == "box":
        widget.geometry_collapsed = Rect(x=x, y=y, w=body_w, h=header_h)
