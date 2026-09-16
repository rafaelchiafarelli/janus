# Task 3: status-bar-wiring

## Status: blocked — depends on task 2

Not scoped in detail yet.

## Proposed contract (draft)

Wire `draw_status_bar` into `janus_switch_screen` and scaffold startup,
same call-site pattern `draw_nav_bar` uses. Update `architecture.md`'s
Stage 2/4/8 sections and `Janus.md`'s field catalog. Flag, as a follow-up
for the ArduinoIHM session (not part of this task), that its three
hand-authored `status_bar` rows should be deleted from the
`.screen.yaml` files once this lands.
