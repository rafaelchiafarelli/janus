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


def layout_screen(screen: Screen, display: DisplayConfig | None = None) -> Screen:
    """`display`, when given, is what a top-level `fill: true` widget
    grows against — without it (today's default), `fill` on any widget
    whose ancestor chain never reaches a known size raises ValueError;
    every other widget lays out exactly as before regardless."""
    avail_w = display.width if display is not None else None
    avail_h = display.height if display is not None else None
    _layout_widget(screen.root, x=0, y=0, avail_w=avail_w, avail_h=avail_h)
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


def _header_height(widget: Widget) -> int:
    """`box`'s header strip is normally `BOX_HEADER_H`, but grows to fit
    the tallest `summary` widget if any are taller than that (a box with
    no `summary` lays out byte-identical to before this existed)."""
    if not widget.summary:
        return BOX_HEADER_H
    tallest = max(_resolve_leaf_size(c)[1] for c in widget.summary)
    return max(BOX_HEADER_H, tallest)


def _summary_width(widget: Widget) -> int:
    """Total width `widget.summary` needs, packed with `GAP` between
    entries — used to widen a `box` past whatever its `children` alone
    would derive, so the (right-aligned) summary row never starts left of
    the box's own x (see `_layout_summary_row`). 0 if there's no summary,
    same zero-cost-when-unused shape as `_distribute_fill`."""
    if not widget.summary:
        return 0
    sizes = [_resolve_leaf_size(c)[0] for c in widget.summary]
    return sum(sizes) + GAP * max(len(sizes) - 1, 0)


def _layout_summary_row(widget: Widget, box_x: int, box_y: int, box_w: int, header_h: int) -> None:
    """Positions `widget.summary` right-aligned within the header strip,
    each vertically centered — the title (drawn separately by the
    runtime, left-aligned) and this row share the same strip. No
    collision check against the title's width — same "no auto-sizing
    text, overflow is an expected v1 case" tradeoff `draw_string`'s own
    clipping already accepts elsewhere."""
    if not widget.summary:
        return
    sizes = [_resolve_leaf_size(c) for c in widget.summary]
    total_w = sum(w for w, _ in sizes) + GAP * max(len(sizes) - 1, 0)
    cursor_x = box_x + box_w - total_w
    for child, (cw, ch) in zip(widget.summary, sizes):
        child.geometry = Rect(x=cursor_x, y=box_y + (header_h - ch) // 2, w=cw, h=ch)
        cursor_x += cw + GAP


def _direction_of(widget: Widget) -> str:
    if widget.layout in ("column", "row"):
        return widget.layout
    if widget.kind in ("column", "row"):
        return widget.kind
    return "column"


def _distribute_fill(
    widget: Widget, direction: str, main_avail: int | None,
    body_avail_w: int | None, body_avail_h: int | None,
) -> dict[int, int]:
    """Measures every non-fill direct child of `widget` (at a throwaway
    position — only `.geometry.w/.h` are read here, the real position pass
    in `_layout_widget` below overwrites `.geometry` for every child
    afterward) to find how much of `main_avail` is left over, then splits
    that evenly among `widget`'s `fill` children (last one absorbs the
    remainder). Keyed by `id(child)` since `Widget` isn't hashable/no
    identity field cheaper than that. Empty dict, no measuring, if `widget`
    has no fill children — the zero-cost path every existing screen takes."""
    fill_children = [c for c in widget.children if c.fill]
    if not fill_children:
        return {}
    if main_avail is None:
        axis = "height" if direction == "column" else "width"
        raise ValueError(
            f"widget {widget.id!r} has a `fill` child but its own {axis} isn't "
            f"known — `fill` needs an unbroken chain of explicit/filled sizes "
            f"back to a screen with `app.display` set"
        )

    total_non_fill_main = 0
    for child in widget.children:
        if child.fill:
            continue
        if direction == "column":
            _layout_widget(child, 0, 0, avail_w=body_avail_w, avail_h=None)
            total_non_fill_main += child.geometry.h
        else:
            _layout_widget(child, 0, 0, avail_w=None, avail_h=body_avail_h)
            total_non_fill_main += child.geometry.w

    gaps = GAP * max(len(widget.children) - 1, 0)
    leftover = main_avail - total_non_fill_main - gaps
    if leftover < 0:
        axis = "height" if direction == "column" else "width"
        raise ValueError(
            f"widget {widget.id!r}'s children already need more {axis} "
            f"({total_non_fill_main + gaps}px) than it has available "
            f"({main_avail}px), before any `fill` child gets a share"
        )

    share, extra = divmod(leftover, len(fill_children))
    return {
        id(child): share + (extra if i == len(fill_children) - 1 else 0)
        for i, child in enumerate(fill_children)
    }


def _layout_widget(
    widget: Widget, x: int, y: int,
    avail_w: int | None = None, avail_h: int | None = None,
    forced_w: int | None = None, forced_h: int | None = None,
) -> None:
    if widget.kind in _LEAF_KINDS:
        w, h = _resolve_leaf_size(widget)
        if forced_w is not None:
            w = forced_w
        if forced_h is not None:
            h = forced_h
        widget.geometry = Rect(x=x, y=y, w=w, h=h)
        return

    # A forced dimension (this widget was itself given a `fill` share by
    # its parent, just below) is an exactly-known size — fold it into
    # `avail` so it's usable the same way a known size from anywhere else
    # would be, when sizing *this* widget's own children.
    own_avail_w = forced_w if forced_w is not None else avail_w
    own_avail_h = forced_h if forced_h is not None else avail_h

    direction = _direction_of(widget)
    header_h = _header_height(widget) if widget.kind == "box" else 0

    body_avail_w = own_avail_w
    body_avail_h = None if own_avail_h is None else own_avail_h - header_h
    main_avail = body_avail_h if direction == "column" else body_avail_w

    fill_sizes = _distribute_fill(widget, direction, main_avail, body_avail_w, body_avail_h)

    cursor_x, cursor_y = x, y + header_h
    for i, child in enumerate(widget.children):
        if i > 0:
            if direction == "column":
                cursor_y += GAP
            else:
                cursor_x += GAP

        child_forced_w = child_forced_h = None
        if child.fill:
            if direction == "column":
                child_forced_h = fill_sizes[id(child)]
            else:
                child_forced_w = fill_sizes[id(child)]
        # cross-axis avail is forwarded to every child as-is (informational
        # ceiling, not a forced size); main-axis avail only matters for a
        # fill child, handled above via forced_w/forced_h instead.
        child_avail_w = body_avail_w if direction == "column" else None
        child_avail_h = body_avail_h if direction == "row" else None

        _layout_widget(
            child, cursor_x, cursor_y,
            avail_w=child_avail_w, avail_h=child_avail_h,
            forced_w=child_forced_w, forced_h=child_forced_h,
        )
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

    w, h = body_w, header_h + body_h
    if widget.kind == "box":
        w = max(w, _summary_width(widget))
    if forced_w is not None:
        w = forced_w
    if forced_h is not None:
        h = forced_h

    widget.geometry = Rect(x=x, y=y, w=w, h=h)
    if widget.kind == "box":
        widget.geometry_collapsed = Rect(x=x, y=y, w=w, h=header_h)
        _layout_summary_row(widget, x, y, w, header_h)
