# Task 2: draw-status-bar

## Status: blocked — depends on task 1

Not scoped in detail yet. Waits on task 1's descriptor shape (rect,
text source, PROGMEM-or-not) being settled and delivered.

## Proposed contract (draft)

`draw_status_bar(app)` in the fixed runtime — static text centered in
the band task 1 reserved. Called from the same sites `draw_nav_bar` is
(never the periodic render tick — see epic.md's acceptance gate 2).
