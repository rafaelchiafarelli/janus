"""Stage 3a — harpia Include emitter. See architecture.md.

Flat only (no sub-messages, v1 decision): walks every screen's widget
tree, collects bindings grouped by harpia message name, dedups fields by
name, and emits one `message {name}{ ... };` block per message.

`collect_bindings` is public because Stage 3b's `emit_bindings_struct.py`
reuses this exact walk to emit the plain-C-struct counterpart of these
same messages (architecture.md, Stage 7's follow-up research pass) —
same fields, independently emitted in two languages for two targets.
"""
from __future__ import annotations

from ..ir import App, Binding, Widget


def collect_bindings(widget: Widget, out: dict[str, dict[str, Binding]]) -> None:
    if widget.bind is not None:
        fields = out.setdefault(widget.bind.message, {})
        existing = fields.get(widget.bind.field)
        if existing is not None and existing.type != widget.bind.type:
            raise ValueError(
                f"conflicting types for {widget.bind.message}.{widget.bind.field}: "
                f"{existing.type!r} vs {widget.bind.type!r} (widget {widget.id!r})"
            )
        fields[widget.bind.field] = widget.bind
    for child in widget.children:
        collect_bindings(child, out)


def emit_harpia(app: App) -> str:
    messages: dict[str, dict[str, Binding]] = {}
    for screen in app.screens:
        collect_bindings(screen.root, messages)

    blocks = []
    for message_name, fields in messages.items():
        field_lines = "\n".join(
            f"    {binding.type} {field_name};" for field_name, binding in fields.items()
        )
        blocks.append(f"message {message_name}{{\n{field_lines}\n}};")

    return "\n\n".join(blocks) + ("\n" if blocks else "")
