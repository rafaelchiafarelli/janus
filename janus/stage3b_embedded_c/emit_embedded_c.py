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

from ..ir import App, Screen, Widget

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

    return (
        f"{{ .kind = {_KIND_ENUM[widget.kind]}, .id = {_c_string(widget.id)}, "
        f".geometry = {_rect(widget.geometry)}, "
        f".geometry_collapsed = {_rect(widget.geometry_collapsed)}, "
        f".initial_expanded = {initial_expanded_c}, "
        f".bind = {{ {bind_c} }}, .action = {action_c}, "
        f".navigate_target = {navigate_target_c}, "
        f".children = {children_array_name}, .child_count = {child_count} }}"
    )


def _emit_widget(
    widget: Widget,
    lines: list[str],
    sv: str,
    counter: list[int],
    screen_index_by_name: dict[str, int] | None,
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
            _emit_widget(c, lines, sv, counter, screen_index_by_name) for c in widget.children
        ]
        counter[0] += 1
        children_array_name = f"{sv}_arr{counter[0]}"
        child_count = len(child_inits)
        body = ",\n".join(f"    {ci}" for ci in child_inits)
        lines.append(
            f"static const janus_widget_desc_t {children_array_name}[] = {{\n{body}\n}};"
        )
    return _widget_init(widget, children_array_name, child_count, screen_index_by_name)


def emit_screen(screen: Screen, screen_index_by_name: dict[str, int] | None = None) -> str:
    """`screen_index_by_name` is only required if this screen has a
    `navigate` button; pass `screen_index_map(app)` once you have the full
    App. Omit it for screens with no navigation."""
    sv = screen_var(screen.name)
    lines: list[str] = []
    counter = [0]

    top_inits = [
        _emit_widget(child, lines, sv, counter, screen_index_by_name)
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
