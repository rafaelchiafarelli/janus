# Epic: label_format

Make `label`/`header` interpolate a live bound value into an authored
`text:` template — `text: "Duty: %d%%"` with `bind: {…, type: int}` on
the same widget renders `Duty: 72%`. Today that widget renders the
literal string `Duty: %d%%` (or, with no `text:`, nothing at all for a
numeric bind).

## Settled decisions

- **Opt-in by overloading `text:`** — no new DSL key. `text:` is a format
  template iff it contains an unescaped `%` conversion **and** the widget
  has a `bind:`. `%%` is a literal `%`. A `text:` with a conversion but
  no `bind:` is a parse-time `ValueError` (nothing to fill; use `%%` for
  a literal percent).
- **Conversion set:** `%d %u %ld %lld %x %s %% %f %.Nf`. Exactly one
  conversion is consumed (the widget has one `bind:`); a second
  conversion in the string is emitted literally. `%s` requires
  `type: string`; the numeric conversions require a numeric bind type;
  mismatch is a parse-time `ValueError`.
- **No libc:** a hand-rolled formatter into a fixed stack buffer.
  `%f`/`%.Nf` use an integer + scaled-fraction split (default 2
  decimals), no `math.h`, no `sprintf`.

## Tasks

| # | file | contract (one line) |
|---|---|---|
| 1 | `tasks/1-format-value-core.md` | `janus_format_into(buf, cap, fmt_flash, bind, bound_struct)` — allocation-free single-arg formatter, no libc, fully unit-tested standalone |
| 2 | `tasks/2-label-header-wiring.md` | Stage 1 detect + validate; `ir.Widget.text_is_format`; Stage 3b bakes `.text_is_format`; `draw_label`/`draw_header` route through the formatter |

## Acceptance gate

1. A `.screen.yaml` `label` with `text: "T: %.1f C"` + a `float` bind
   generates, and `test_runtime.c` renders it (via the mock font/pixel
   log) as `T: 21.5 C` for a bound value of `21.5`.
2. `text: "%d%%"` + int bind `72` → `72%`. `text: "100%%"` + **no** bind
   → literal `100%` (still valid — `%%` only). `text: "%d"` + no bind →
   `ValueError` at parse.
3. Python `unittest` + C `ctest` green; `scripts/avr_gate.sh` green (the
   formatter's stack buffer is small and bounded — assert `.bss`
   unchanged).

## Out of scope

- Multiple arguments / multiple binds per widget.
- `button` / `box`-header format text (buttons are unbound in v1; revisit
  only if a real need appears).
- Width / flag / `*` precision (`%5d`, `%-8s`, `%+d`) — only `%.Nf` takes
  a precision, everything else is bare.
- Locale, grouping, `%e`/`%g`.
