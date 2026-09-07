# Task 1: format-value-core

## Contract

A standalone, allocation-free, single-argument value formatter in the
fixed runtime. No libc `printf`/`sprintf`, no `math.h`. This task ships
the function and its tests only — no widget wiring (task 2).

### In

Called by the runtime with a flash-resident format string and one
binding.

### Delivered

New pair `runtime/embedded_c/src/janus_format.c` +
`runtime/embedded_c/include/janus_format.h` (one concern per file, same
pattern as `janus_font.*`), added to `runtime/embedded_c/CMakeLists.txt`
(`janus_runtime` sources + a `janus_format_tests` ctest target, mirroring
`janus_font_tests`).

```c
/* Writes at most cap-1 chars + NUL into buf. Returns strlen(buf).
 * `fmt` is flash-resident on AVR (JANUS_PGM_READ_U8 per byte, same as
 * draw_string's from_flash path). Consumes exactly one conversion using
 * `bind` + `bound_struct` (via the existing read_bound_value /
 * read_bound_string — those move to janus_runtime-internal shared use,
 * or are duplicated minimally here; implementer's call, keep one owner).
 * A second conversion, or any conversion when bind->field_type ==
 * JANUS_FIELD_NONE, is copied through literally. Unknown `%x` letter ->
 * literal. Always NUL-terminates, never writes past cap. */
size_t janus_format_into(char *buf, size_t cap, const char *fmt,
                         const janus_bind_t *bind, const void *bound_struct);
```

Supported conversions: `%d %u %ld %lld %x %s %% %f %.Nf`

- numeric conversions pull `read_bound_value` (double) and cast:
  `%d`→`long`, `%u`→`unsigned long`, `%ld`/`%lld`→`long long`,
  `%x`→`unsigned long` hex (lowercase, no `0x`).
- `%s` pulls `read_bound_string`; `NULL` → the literal text `(null)`
  (matches glibc, keeps a missing bound string visible not silent).
- `%f` → 6? no — **default 2 decimals**; `%.Nf` → `N` decimals, `N` in
  `0..9`. Formatting: `whole = (long long)v; frac = llround((v - whole) *
  pow10[N])` done with an integer `pow10` table; handle the carry when
  `frac == pow10[N]`; negative values print one leading `-` and use
  `|v|`. No rounding-to-even, plain round-half-away-from-zero.
- `%%` → one `%`.

`read_bound_value` / `read_bound_string` currently `static` in
`janus_runtime.c` — promote to non-`static` with declarations in a
shared internal header (`janus_runtime.h` is the public one; add
`runtime/embedded_c/src/janus_bound.h` **or** just declare them in
`janus_format.h` since it already needs `janus_bind_t`). One owner, no
copy-paste of the offset math.

### Not in scope

- Any widget descriptor / Stage 3b / Stage 1 change (task 2).
- Thread-safety / reentrancy beyond "caller owns `buf`".
- `long double`, `%e`, `%g`, width, flags, `*`.

## Dependencies

None beyond the current runtime. Does **not** depend on the
`draw_primitives` epic.

## Pre-work

None. Tests are a new `test_format.c` with hand-built `janus_bind_t` +
a local struct as `bound_struct`, no fixtures.

## Tests (`runtime/embedded_c/tests/test_format.c`, new ctest target)

- `%d` of int 72 → `"72"`; of int -5 → `"-5"`.
- `"Duty: %d%%"` , int 72 → `"Duty: 72%"`.
- `%u` of -1-as-int reads the double path → document + assert the actual
  behaviour (value is `4294967295` via `(unsigned long)(double)`), so the
  test pins it rather than leaving it undefined.
- `%x` of 255 → `"ff"`.
- `%ld` / `%lld` of a large int64 bound value round-trips exactly for
  values within double's integer-exact range; add a comment that beyond
  2^53 it's lossy by construction (read path is `double`).
- `%.1f` of 21.5 → `"21.5"`; `%.0f` of 2.6 → `"3"`; `%f` (default) of
  -0.005 → `"-0.01"` (carry + sign); `%.2f` of 9.999 → `"10.00"` (carry
  ripples to whole).
- `%s` of a set `const char *` → that string; of `NULL` → `"(null)"`.
- cap: `janus_format_into(buf, 4, "%d", …, 12345)` → `buf == "123"`,
  return 3, no overflow (guard with a canary byte).
- no bind: `field_type == JANUS_FIELD_NONE`, `"x=%d"` → `"x=%d"` verbatim.
- second conversion: `"%d %d"` → `"72 %d"`.

## DoD

Contract delivered · `ctest` green (incl. new `janus_format_tests`) ·
`scripts/avr_gate.sh` green · no Python file touched · this file renamed
`1-format-value-core-done.md` · commit + merge
`1-format-value-core → tasks`.
