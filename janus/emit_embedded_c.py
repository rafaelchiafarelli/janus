"""Stage 3b — embedded-C data emitter. See architecture.md.

Scope of this module: the widget descriptor arrays + janus_screen_desc_t
for ONE screen, as plain C text. Not yet in scope (later increments):
the .tmpl wrapper (#includes, file header), janus_app.gen.c, and the
action-ID enum.

Struct-binding convention used below (`offsetof({message}_t, {field})`,
one struct type per harpia message) is PROVISIONAL — it depends on
architecture.md Stage 7's open question (does harpia's ZmqAdapter emit a
plain C struct at all?). Emitting the text doesn't require that question
to be resolved yet; only actually compiling the output does.
"""
from __future__ import annotations

from .ir import Screen, Widget

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


def _widget_init(widget: Widget, children_array_name: str, child_count: int) -> str:
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

    return (
        f"{{ .kind = {_KIND_ENUM[widget.kind]}, .id = {_c_string(widget.id)}, "
        f".geometry = {_rect(widget.geometry)}, "
        f".geometry_collapsed = {_rect(widget.geometry_collapsed)}, "
        f".bind = {{ {bind_c} }}, .action = JANUS_ACTION_NONE, "
        f".children = {children_array_name}, .child_count = {child_count} }}"
    )


def _emit_widget(widget: Widget, lines: list[str], screen_var: str, counter: list[int]) -> str:
    """Emits (via `lines`) this widget's own children array, if it has
    any — children first, so a container's array is only ever referenced
    after it's been declared (post-order, no forward declarations needed
    since widget trees have no cycles). Returns this widget's own
    initializer expression.
    """
    children_array_name = "NULL"
    child_count = 0
    if widget.children:
        child_inits = [_emit_widget(c, lines, screen_var, counter) for c in widget.children]
        counter[0] += 1
        children_array_name = f"{screen_var}_arr{counter[0]}"
        child_count = len(child_inits)
        body = ",\n".join(f"    {ci}" for ci in child_inits)
        lines.append(
            f"static const janus_widget_desc_t {children_array_name}[] = {{\n{body}\n}};"
        )
    return _widget_init(widget, children_array_name, child_count)


def emit_screen(screen: Screen) -> str:
    screen_var = screen.name.lower().replace("-", "_")
    lines: list[str] = []
    counter = [0]

    top_inits = [
        _emit_widget(child, lines, screen_var, counter) for child in screen.root.children
    ]
    widgets_array = f"{screen_var}_widgets"
    body = ",\n".join(f"    {init}" for init in top_inits)
    lines.append(f"static const janus_widget_desc_t {widgets_array}[] = {{\n{body}\n}};")

    lines.append(
        f"const janus_screen_desc_t {screen_var}_screen = {{\n"
        f"    .name = {_c_string(screen.name)},\n"
        f"    .widgets = {widgets_array},\n"
        f"    .widget_count = {len(top_inits)},\n"
        f"}};"
    )
    return "\n\n".join(lines) + "\n"
