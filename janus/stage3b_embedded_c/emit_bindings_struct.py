"""Stage 3b — bindings struct emitter. See architecture.md, Stage 7's
follow-up research pass.

harpia's ZmqAdapter can never produce this (protobuf's C++ classes only,
confirmed down to the `protoc --cpp_out` invocation itself — no plain C
struct path exists anywhere in harpia). So this is Janus's own output:
one plain C struct + one instance per harpia message referenced by any
widget's `bind`, generated from the exact same walk `emit_harpia.py`
uses for the `.harpia` Include (`collect_bindings`) — same fields,
independently emitted in two languages for two targets.

Instances are zero-initialized (`= {0}`); Janus generates the shape, not
the data — populating real values at runtime is firmware's job (the same
split Stage 5/8 already use for action dispatch and the event loop).

Per-field dirty tracking (added 2026-09-05): alongside `{message}_t`,
every message also gets a `{message}_dirty_t` — one `bool` per field,
same names — and a zero-initialized `{message}_dirty` instance.
Firmware sets `{message}_dirty.{field} = true` whenever it writes a new
value into `{message}_instance.{field}`; a dirty-aware render pass
(`janus_render_widget_if_dirty`/`janus_render_screen_if_dirty`, see
architecture.md Stage 4) reads and clears it via the exact same
`offsetof`-into-a-generated-struct mechanism `janus_bind_t.field_offset`
already uses for the *value* struct — `janus_bind_t.dirty_offset` is the
same idea, pointed at this struct instead. Deliberately per *field*, not
per *widget instance*: two widgets bound to the same field share one
dirty bit, which is what "did the data change" actually means here.
"""
from __future__ import annotations

from ..ir import App, Binding
from ..stage3a_harpia.emit_harpia import collect_bindings

_C_TYPE = {
    "string": "const char *",
    "int": "int",
    "int64": "int64_t",
    "float": "float",
}


def _collect_messages(app: App) -> dict[str, dict[str, Binding]]:
    messages: dict[str, dict[str, Binding]] = {}
    for screen in app.screens:
        collect_bindings(screen.root, messages)
    return messages


def emit_bindings_header(app: App) -> str:
    messages = _collect_messages(app)
    if not messages:
        return ""

    needs_stdint = any(
        binding.type == "int64" for fields in messages.values() for binding in fields.values()
    )
    include = "#include <stdbool.h>\n" + ("#include <stdint.h>\n" if needs_stdint else "") + "\n"

    blocks = []
    for message_name, fields in messages.items():
        field_lines = "\n".join(
            f"    {_C_TYPE[binding.type]} {field_name};" for field_name, binding in fields.items()
        )
        dirty_lines = "\n".join(f"    bool {field_name};" for field_name in fields)
        blocks.append(
            f"typedef struct {{\n{field_lines}\n}} {message_name}_t;\n\n"
            f"extern {message_name}_t {message_name}_instance;\n\n"
            f"typedef struct {{\n{dirty_lines}\n}} {message_name}_dirty_t;\n\n"
            f"extern {message_name}_dirty_t {message_name}_dirty;"
        )

    return include + "\n\n".join(blocks) + ("\n" if blocks else "")


def emit_bindings_source(app: App) -> str:
    messages = _collect_messages(app)
    lines = []
    for message_name in messages:
        lines.append(f"{message_name}_t {message_name}_instance = {{0}};")
        lines.append(f"{message_name}_dirty_t {message_name}_dirty = {{0}};")
    return "\n".join(lines) + ("\n" if lines else "")
