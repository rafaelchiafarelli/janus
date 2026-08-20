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

    needs_stdint = any(
        binding.type == "int64" for fields in messages.values() for binding in fields.values()
    )
    include = "#include <stdint.h>\n\n" if needs_stdint else ""

    blocks = []
    for message_name, fields in messages.items():
        field_lines = "\n".join(
            f"    {_C_TYPE[binding.type]} {field_name};" for field_name, binding in fields.items()
        )
        blocks.append(
            f"typedef struct {{\n{field_lines}\n}} {message_name}_t;\n\n"
            f"extern {message_name}_t {message_name}_instance;"
        )

    return include + "\n\n".join(blocks) + ("\n" if blocks else "")


def emit_bindings_source(app: App) -> str:
    messages = _collect_messages(app)
    lines = [f"{message_name}_t {message_name}_instance = {{0}};" for message_name in messages]
    return "\n".join(lines) + ("\n" if lines else "")
