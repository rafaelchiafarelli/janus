"""Stage 3b — embedded-C data emitter. See architecture.md.

In scope: per-screen widget descriptor arrays + janus_screen_desc_t
(`emit_screen`), the action-ID enum (`emit_actions_header`), and the
app-level screen/nav table (`emit_app_table`). The .tmpl wrapper that
turns these into complete, standalone files lives in `emit_files.py`.

Struct-binding convention used below (`offsetof({message}_t, {field})`,
one struct type per harpia message) is Janus's own — confirmed by Stage
7's research pass that harpia's ZmqAdapter emits C++ only (protobuf
classes, never a plain C struct), so this struct is never harpia's
output to consume. `janus_bindings.h` is hand-written for now
(`examples/host_demo`); generating it directly from these same Bindings
is the natural next increment here.
"""
from __future__ import annotations

from ..ir import App, DisplayConfig, Screen, Widget

_KIND_ENUM = {
    "label": "JANUS_WIDGET_LABEL",
    "header": "JANUS_WIDGET_HEADER",
    "button": "JANUS_WIDGET_BUTTON",
    "image": "JANUS_WIDGET_IMAGE",
    "progress": "JANUS_WIDGET_PROGRESS",
    "gauge": "JANUS_WIDGET_GAUGE",
    "checkbox": "JANUS_WIDGET_CHECKBOX",
    "radiobutton": "JANUS_WIDGET_RADIOBUTTON",
    "radiogroup": "JANUS_WIDGET_RADIOGROUP",
    "led": "JANUS_WIDGET_LED",
    "box": "JANUS_WIDGET_BOX",
    "column": "JANUS_WIDGET_COLUMN",
    "row": "JANUS_WIDGET_ROW",
    # navlist is styling sugar over column (Janus.md catalog) — no distinct enum value
    "navlist": "JANUS_WIDGET_COLUMN",
    "divider": "JANUS_WIDGET_DIVIDER",
    "toggle": "JANUS_WIDGET_TOGGLE",
    "badge": "JANUS_WIDGET_BADGE",
    "slider": "JANUS_WIDGET_SLIDER",
}

_FIELD_TYPE_ENUM = {
    "string": "JANUS_FIELD_STRING",
    "int": "JANUS_FIELD_INT",
    "int64": "JANUS_FIELD_INT64",
    "float": "JANUS_FIELD_FLOAT",
}

_DISPLAY_COLOR_MACRO = {
    "mono": "JANUS_DISPLAY_COLOR_MONO",
    "gray": "JANUS_DISPLAY_COLOR_GRAY",
    "rgb565": "JANUS_DISPLAY_COLOR_RGB565",
}
# every value defined regardless of selection, so driver code can
# `#if JANUS_DISPLAY_COLOR == JANUS_DISPLAY_COLOR_RGB565`
_DISPLAY_COLOR_VALUES = ["JANUS_DISPLAY_COLOR_MONO", "JANUS_DISPLAY_COLOR_GRAY", "JANUS_DISPLAY_COLOR_RGB565"]

_DISPLAY_BUS_MACRO = {
    "spi": "JANUS_DISPLAY_BUS_SPI",
    "i2c": "JANUS_DISPLAY_BUS_I2C",
    "parallel": "JANUS_DISPLAY_BUS_PARALLEL",
}
_DISPLAY_BUS_VALUES = ["JANUS_DISPLAY_BUS_SPI", "JANUS_DISPLAY_BUS_I2C", "JANUS_DISPLAY_BUS_PARALLEL"]

_DISPLAY_CONTROLLER_MACRO = {
    "st7789": "JANUS_DISPLAY_CONTROLLER_ST7789",
    "st7789v": "JANUS_DISPLAY_CONTROLLER_ST7789V",
    "ili9341": "JANUS_DISPLAY_CONTROLLER_ILI9341",
    "ili9341v": "JANUS_DISPLAY_CONTROLLER_ILI9341V",
    "hx8357": "JANUS_DISPLAY_CONTROLLER_HX8357",
    "gc9a01": "JANUS_DISPLAY_CONTROLLER_GC9A01",
    "ssd1306": "JANUS_DISPLAY_CONTROLLER_SSD1306",
    "sh1106": "JANUS_DISPLAY_CONTROLLER_SH1106",
    "il3820": "JANUS_DISPLAY_CONTROLLER_IL3820",
    "il0373": "JANUS_DISPLAY_CONTROLLER_IL0373",
}
# first-defined order, kept stable regardless of dict iteration guarantees
_DISPLAY_CONTROLLER_VALUES = [
    "JANUS_DISPLAY_CONTROLLER_ST7789", "JANUS_DISPLAY_CONTROLLER_ST7789V",
    "JANUS_DISPLAY_CONTROLLER_ILI9341", "JANUS_DISPLAY_CONTROLLER_ILI9341V",
    "JANUS_DISPLAY_CONTROLLER_HX8357", "JANUS_DISPLAY_CONTROLLER_GC9A01",
    "JANUS_DISPLAY_CONTROLLER_SSD1306", "JANUS_DISPLAY_CONTROLLER_SH1106",
    "JANUS_DISPLAY_CONTROLLER_IL3820", "JANUS_DISPLAY_CONTROLLER_IL0373",
]


def _c_string(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _rect(r) -> str:
    return f"{{{r.x}, {r.y}, {r.w}, {r.h}}}" if r is not None else "{0, 0, 0, 0}"


def screen_var(name: str) -> str:
    return name.lower().replace("-", "_")


def screen_index_map(app: App) -> dict[str, int]:
    return {screen.name: i for i, screen in enumerate(app.screens)}


# ---------------------------------------------------------------- actions --

def action_enum_name(action: str) -> str:
    return f"JANUS_ACTION_{action.upper()}"


def _collect_on_press(widget: Widget, seen: dict[str, None]) -> None:
    if widget.on_press is not None:
        seen[widget.on_press] = None
    for child in widget.children:
        _collect_on_press(child, seen)


def collect_on_press_actions(app: App) -> list[str]:
    """First-seen order, deduped — `navigate` values never appear here
    (Stage 5: they're handled directly by the runtime, not the action
    enum)."""
    seen: dict[str, None] = {}
    for screen in app.screens:
        _collect_on_press(screen.root, seen)
    return list(seen)


def screen_on_press_actions(screen: Screen) -> list[str]:
    """Same as `collect_on_press_actions`, scoped to one screen — used to
    decide whether that screen's generated .c file needs to include
    `janus_actions.gen.h` at all."""
    seen: dict[str, None] = {}
    _collect_on_press(screen.root, seen)
    return list(seen)


def _collect_bound_messages(widget: Widget, seen: dict[str, None]) -> None:
    if widget.bind is not None:
        seen[widget.bind.message] = None
    for child in widget.children:
        _collect_bound_messages(child, seen)


def screen_bound_messages(screen: Screen) -> list[str]:
    """Distinct `Binding.message` names used anywhere in this screen,
    first-seen order. Stage 3b v1 assumes at most one per screen (see
    `emit_screen`'s `.bound_struct` resolution)."""
    seen: dict[str, None] = {}
    _collect_bound_messages(screen.root, seen)
    return list(seen)


# ------------------------------------------------------------- focus order --
# Stage 6: encoder/button navigation needs a traversal order for "focusable"
# widgets, baked at generation time (same "push work to build time" spirit
# as geometry) rather than re-derived by the runtime. "Focusable" here is
# deliberately the exact same set touch already dispatches on — box (its
# header always toggles) and any leaf with `on_press`/`navigate` set — so
# this needs no new YAML authoring, just a pre-order walk matching the
# runtime's own traversal order (janus_runtime.c's render_widget /
# janus_input_touch.c's hit_test_widget), which is NOT the same order
# _emit_widget below emits C in (that one is post-order, for forward
# declarations). 255 (JANUS_FOCUS_NONE in the C header) marks "not
# focusable" — fine for realistic screen sizes (a uint8_t sentinel).
_STRUCTURAL_KINDS = {"column", "row", "radiogroup", "navlist"}
FOCUS_ORDER_NONE = 255


def _is_focusable(widget: Widget) -> bool:
    if widget.kind == "box":
        return True
    if widget.kind in _STRUCTURAL_KINDS:
        return False
    return widget.on_press is not None or widget.navigate is not None


def _assign_focus_order(root: Widget) -> dict[int, int]:
    order: dict[int, int] = {}
    counter = [0]

    def visit(w: Widget) -> None:
        if _is_focusable(w):
            order[id(w)] = counter[0]
            counter[0] += 1
        for child in w.children:
            visit(child)

    visit(root)
    return order


def emit_actions_header(app: App) -> str:
    values = ["JANUS_ACTION_NONE"] + [
        action_enum_name(a) for a in collect_on_press_actions(app)
    ]
    body = ",\n".join(f"    {v}" for v in values)
    return (
        f"typedef enum {{\n{body}\n}} janus_action_t;\n\n"
        f"void janus_handle_action(janus_action_t action);\n"
    )


# ------------------------------------------------------------- per-screen --

def _widget_init(
    widget: Widget,
    children_array_name: str,
    child_count: int,
    screen_index_by_name: dict[str, int] | None,
    focus_order_map: dict[int, int],
) -> str:
    if widget.bind is not None:
        struct_type = f"{widget.bind.message}_t"
        range_min, range_max = widget.range if widget.range is not None else (0, 0)
        bind_c = (
            f".field_offset = offsetof({struct_type}, {widget.bind.field}), "
            f".field_type = {_FIELD_TYPE_ENUM[widget.bind.type]}, "
            f".range_min = {range_min}, .range_max = {range_max}"
        )
    else:
        bind_c = ".field_type = JANUS_FIELD_NONE"

    action_c = action_enum_name(widget.on_press) if widget.on_press is not None else "JANUS_ACTION_NONE"

    if widget.navigate is not None:
        if screen_index_by_name is None or widget.navigate not in screen_index_by_name:
            raise ValueError(
                f"button {widget.id!r} navigates to {widget.navigate!r}, which isn't in "
                f"screen_index_by_name — build it from the full App (screen_index_map)"
            )
        navigate_target_c = str(screen_index_by_name[widget.navigate])
    else:
        navigate_target_c = "-1"

    initial_expanded_c = "true" if widget.default_expanded else "false"
    static_text_c = _c_string(widget.text) if widget.text is not None else "NULL"
    focus_order_c = str(focus_order_map.get(id(widget), FOCUS_ORDER_NONE))

    return (
        f"{{ .kind = {_KIND_ENUM[widget.kind]}, .id = {_c_string(widget.id)}, "
        f".static_text = {static_text_c}, "
        f".geometry = {_rect(widget.geometry)}, "
        f".geometry_collapsed = {_rect(widget.geometry_collapsed)}, "
        f".initial_expanded = {initial_expanded_c}, "
        f".bind = {{ {bind_c} }}, .action = {action_c}, "
        f".navigate_target = {navigate_target_c}, "
        f".focus_order = {focus_order_c}, "
        f".children = {children_array_name}, .child_count = {child_count} }}"
    )


def _emit_widget(
    widget: Widget,
    lines: list[str],
    sv: str,
    counter: list[int],
    screen_index_by_name: dict[str, int] | None,
    focus_order_map: dict[int, int],
) -> str:
    """Emits (via `lines`) this widget's own children array, if it has
    any — children first, so a container's array is only ever referenced
    after it's been declared (post-order, no forward declarations needed
    since widget trees have no cycles). Returns this widget's own
    initializer expression.
    """
    children_array_name = "NULL"
    child_count = 0
    if widget.children:
        child_inits = [
            _emit_widget(c, lines, sv, counter, screen_index_by_name, focus_order_map)
            for c in widget.children
        ]
        counter[0] += 1
        children_array_name = f"{sv}_arr{counter[0]}"
        child_count = len(child_inits)
        body = ",\n".join(f"    {ci}" for ci in child_inits)
        lines.append(
            f"static const janus_widget_desc_t {children_array_name}[] = {{\n{body}\n}};"
        )
    return _widget_init(widget, children_array_name, child_count, screen_index_by_name, focus_order_map)


def emit_screen(screen: Screen, screen_index_by_name: dict[str, int] | None = None) -> str:
    """`screen_index_by_name` is only required if this screen has a
    `navigate` button; pass `screen_index_map(app)` once you have the full
    App. Omit it for screens with no navigation."""
    sv = screen_var(screen.name)
    lines: list[str] = []
    counter = [0]
    focus_order_map = _assign_focus_order(screen.root)

    top_inits = [
        _emit_widget(child, lines, sv, counter, screen_index_by_name, focus_order_map)
        for child in screen.root.children
    ]
    widgets_array = f"{sv}_widgets"
    body = ",\n".join(f"    {init}" for init in top_inits)
    lines.append(f"static const janus_widget_desc_t {widgets_array}[] = {{\n{body}\n}};")

    messages = screen_bound_messages(screen)
    if len(messages) > 1:
        raise ValueError(
            f"screen {screen.name!r} binds more than one message "
            f"({messages!r}) — Stage 3b v1 assumes one bound message per "
            f"screen (see architecture.md Stage 3b/7)"
        )
    bound_struct_c = f"&{messages[0]}_instance" if messages else "NULL"

    lines.append(
        f"const janus_screen_desc_t {sv}_screen = {{\n"
        f"    .name = {_c_string(screen.name)},\n"
        f"    .widgets = {widgets_array},\n"
        f"    .widget_count = {len(top_inits)},\n"
        f"    .bound_struct = {bound_struct_c},\n"
        f"}};"
    )
    return "\n\n".join(lines) + "\n"


# --------------------------------------------------------- display config --

def emit_display_config(display: DisplayConfig) -> str:
    """Plain data, consumed by hand-written vendor driver code (never by
    the fixed runtime library, which is display-size-agnostic — see
    architecture.md). Only called when `app.display` is set.

    `bus`/`controller` are optional independently of `display` itself —
    a project may declare panel size/color before picking real hardware.
    The full enumeration (every known bus/controller value) is always
    emitted so driver code can `#if JANUS_DISPLAY_CONTROLLER == ...`
    regardless of whether a selection was made; the selection macro
    itself (`JANUS_DISPLAY_BUS`/`JANUS_DISPLAY_CONTROLLER`) is only
    emitted when that field is set — undefined, not defaulted, matches
    the "validate, don't silently default" rule used elsewhere."""
    color_defines = "\n".join(
        f"#define {name} {i}" for i, name in enumerate(_DISPLAY_COLOR_VALUES)
    )
    bus_defines = "\n".join(
        f"#define {name} {i}" for i, name in enumerate(_DISPLAY_BUS_VALUES)
    )
    controller_defines = "\n".join(
        f"#define {name} {i}" for i, name in enumerate(_DISPLAY_CONTROLLER_VALUES)
    )

    parts = [
        f"#define JANUS_DISPLAY_WIDTH {display.width}\n"
        f"#define JANUS_DISPLAY_HEIGHT {display.height}\n",
        f"{color_defines}\n"
        f"#define JANUS_DISPLAY_COLOR {_DISPLAY_COLOR_MACRO[display.color]}\n",
        f"{bus_defines}\n" + (
            f"#define JANUS_DISPLAY_BUS {_DISPLAY_BUS_MACRO[display.bus]}\n"
            if display.bus is not None else ""
        ),
        f"{controller_defines}\n" + (
            f"#define JANUS_DISPLAY_CONTROLLER {_DISPLAY_CONTROLLER_MACRO[display.controller]}\n"
            if display.controller is not None else ""
        ),
    ]
    return "\n".join(parts)


# ------------------------------------------------------------- app table --

def emit_app_table(app: App) -> str:
    screen_vars = [screen_var(s.name) for s in app.screens]

    lines = [
        "\n".join(f"extern const janus_screen_desc_t {sv}_screen;" for sv in screen_vars),
        "static const janus_screen_desc_t *const janus_app_screens[] = {\n"
        + ",\n".join(f"    &{sv}_screen" for sv in screen_vars)
        + "\n};",
    ]

    if app.nav is not None:
        title_by_screen = {t.screen: t.title for t in app.nav}
        missing = [s.name for s in app.screens if s.name not in title_by_screen]
        if missing:
            raise ValueError(f"app.nav is missing a title for screen(s): {missing}")
        titles_body = ",\n".join(
            f"    {_c_string(title_by_screen[s.name])}" for s in app.screens
        )
        lines.append(
            f"static const char *const janus_app_nav_titles[] = {{\n{titles_body}\n}};"
        )
        titles_ref = "janus_app_nav_titles"
    else:
        titles_ref = "NULL"

    lines.append(
        "janus_app_t janus_app = {\n"
        "    .screens = janus_app_screens,\n"
        f"    .nav_titles = {titles_ref},\n"
        f"    .screen_count = {len(screen_vars)},\n"
        "    .active_screen = 0,\n"
        "};"
    )
    return "\n\n".join(lines) + "\n"
