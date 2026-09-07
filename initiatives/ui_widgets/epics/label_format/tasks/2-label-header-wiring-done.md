# Task 2: label-header-wiring

## Contract

Wire `janus_format_into` (task 1) into `label`/`header` rendering, driven
by a build-time flag baked from Stage 1's recognition of a format
`text:`.

### In

`.screen.yaml`:

```yaml
- kind: label
  bind: { message: pwm, field: ch0_duty_percent, type: int }
  text: "Duty: %d%%"
```

### Delivered

**`janus/ir.py`** — `Widget.text_is_format: bool = False` (always set by
the parser, never authored; documented like `hidden`).

**`janus/stage1_parse/dsl_yaml.py`**:
- module regex `_FORMAT_CONV_RE` matching one conversion:
  `%(\.\d)?(d|u|ld|lld|x|s|f)` — i.e. a `%` **not** part of `%%`.
- in `_parse_widget`, after building the widget: `text_is_format =
  widget.text is not None and _FORMAT_CONV_RE.search(_strip_pct_pct(widget.text))`
  where `_strip_pct_pct` removes `%%` pairs first so `"100%%"` is not a
  format string.
- `_validate_widget` additions (only when `text_is_format`):
  - `widget.kind` must be `label` or `header` → else `ValueError`
    (`"format text: is only supported on label/header"`).
  - `widget.bind is not None` → else `ValueError`
    (`"text: %r has a format conversion but no bind: to fill it (use %% for a literal percent)"`).
  - conversion vs bind type: `%s` ⇔ `type == "string"`; any numeric
    conversion ⇔ `type in {int,int64,float}` → else `ValueError`.
  - more than one conversion → **warning** via `log`, not an error
    (runtime emits the 2nd literally); keep parse permissive here.

**`janus/stage3b_embedded_c/emit_embedded_c.py`** — `_widget_init` bakes
`.text_is_format = {true|false}` into the descriptor initializer (next to
`.static_text`).

**`runtime/embedded_c/include/janus_runtime.h`** —
`janus_widget_desc_t`: add `bool text_is_format;` (documented: when true,
`static_text` is a flash format string consumed by `janus_format_into`
with this widget's `bind`).

**`runtime/embedded_c/src/janus_runtime.c`** — `#include "janus_format.h"`;
in `draw_label` and `draw_header`:
- if `lw.text_is_format`: `char buf[JANUS_FORMAT_BUF]; janus_format_into(buf,
  sizeof buf, lw.static_text, &lw.bind, bound_struct);` then
  `draw_string(lw.geometry, buf, lw.color, lw.bg_color, /*from_flash=*/false,
  lw.font_size, lw.font_scale);`
- else: exactly today's path (static_text-from-flash, else bound string).
- `#define JANUS_FORMAT_BUF 48` in `janus_format.h` — one stack buffer,
  bounded, documented as the max rendered label length (longer → clipped
  by `janus_format_into`'s `cap`, then by `draw_string`'s own right-edge
  clip anyway).

**`Janus.md`** — `label`/`header` catalog rows note the `text:` +
`bind:` format-template behaviour and the supported conversions; link the
`%%` rule.

**`architecture.md`** — Stage 4 glyph section: one paragraph that a
`text_is_format` label formats into a stack buffer then draws
`from_flash=false`, and Stage 1 gets a line under its `text:` handling.

### Not in scope

- `button`/`box` format text.
- Re-baking `static_text` — it stays the raw template in flash; the
  runtime formats per draw (a label already redraws per tick via the
  dirty path, so this is where live value substitution belongs).

## Dependencies

- **label_format task 1** (`janus_format_into`, `janus_format.h`,
  `JANUS_FORMAT_BUF`) merged to `tasks`.

## Pre-work

None. New Python tests use inline YAML/dict fixtures; new C test reuses
the mock font pixel-log helper added for existing `draw_string` tests.

## Tests

Python (`tests/test_dsl_yaml*.py`, `tests/test_emit_embedded_c.py`):
- `"Duty: %d%%"` + int bind → `widget.text_is_format is True`.
- `"100%%"` + no bind → `text_is_format is False`, no error.
- `"%d"` + no bind → `ValueError`.
- `"%s"` + int bind → `ValueError`; `"%d"` + string bind → `ValueError`.
- emit: descriptor initializer contains `.text_is_format = true` for the
  format label, `= false` for a plain one.

C (`runtime/embedded_c/tests/test_runtime.c`):
- format `label` with `"%d%%"` + bound int 72 renders glyphs spelling
  `72%` (assert against the mock pixel log / a font-decode helper).
- `"T: %.1f C"` + bound float 21.5 → `T: 21.5 C`.
- a plain `label` (no conversion) with both `text:` and a string `bind:`
  still renders `static_text` (tie-break unchanged).

## DoD

Contract delivered · `python -m unittest discover -s tests` green ·
`ctest` green · `scripts/avr_gate.sh` green · docs updated
(`Janus.md`, `architecture.md`) · this file renamed
`2-label-header-wiring-done.md` · commit + merge
`2-label-header-wiring → tasks`.
