# Initiative: shared_field_dirty

Make the dirty-aware repaint (`janus_render_screen_if_dirty`,
`janus_render_widget_if_dirty`) repaint **every** widget bound to a changed
field, not only the first one the sweep reaches.

Reported 2026-09-26 from ArduinoIHM (the handoff there:
`ArduinoIHM/IHM/initiatives/janus_handoff/2026-09-26-dirty-flag-shared-field.md`).
Rafael chose to fix it here rather than keep an ArduinoIHM-side workaround.

## The bug

`bind_consume_dirty()` (`runtime/embedded_c/src/janus_runtime.c`) **tests
and clears** a field's dirty flag in one step, per widget. The flag belongs
to the *field* (one `bool` per field in the generated `*_dirty_t`), but it
is consumed by whichever widget `render_widget` reaches first. Every later
widget bound to the same field sees it clear and is skipped.

Seen on ArduinoIHM hardware: its PWM tab binds each channel's `led` **and**
its Enabled `toggle` to one field (`pwm.ch0_enabled`), and the Relay tab
binds each `toggle` and its `led` to `relay.relay_N`. After a change plus
`janus_render_screen_if_dirty()`:

| Screen / rows | Tree order | Repaints | Stays stale |
|---|---|---|---|
| PWM CH0/CH1 | led, toggle | led | toggle |
| PWM CH2/CH3, Relay | toggle, led | toggle | led |

Two widgets on one field is ordinary yaml (an indicator next to its
control), so the runtime has to support it.

## Epics

| Epic | Scope |
|---|---|
| `dirty_sweep` | Change when dirty flags are cleared so one sweep repaints all widgets bound to a field; native tests; docs. |

## Downstream (not Janus work, for the ArduinoIHM session afterwards)

After a Janus release with this fix, regenerate ArduinoIHM's `lib/GUI` and:
- remove the full `janus_render_screen()` workaround after an RE2 switch
  change in `src/main.cpp` (navigation `value_editing/1`);
- the PC path (`PWM_CHANNEL_CONFIG` -> `janus_render_screen_if_dirty`) then
  updates both widgets with no ArduinoIHM change.

## Branch chain

```
dev -> features -> shared_field_dirty -> epics -> dirty_sweep -> tasks -> 1-sweep-level-dirty-clear
```

This clone already has a stale `tasks` branch from an earlier chain. Check
that it's merged before reusing the name.
