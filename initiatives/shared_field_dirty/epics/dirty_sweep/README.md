# Epic: dirty_sweep

One sweep of the dirty-aware repaint draws every widget whose bound field
is dirty, however many widgets share that field. See the initiative README
for the bug and the ArduinoIHM evidence.

## Tasks

```
1-sweep-level-dirty-clear   test flags during the sweep, clear after it; tests; docs   (no deps)
```

## Acceptance gate

- Full test suite passes (runtime ctest + Python tests).
- New native test: two leaves bound to one field, marked dirty, one
  `janus_render_screen_if_dirty` -> both draw, and the flag is clear
  afterwards.
