# Task 1: status-bar-field-and-layout

## Status: blocked — not ready to implement

This task's contract depends on decisions listed under "Open questions
to settle with Rafael before task 1 starts" in `../epic.md` (the
`app.yaml` field shape, whether `display.size` becomes required, and the
`STATUS_BAR_H`/colour constants). Per this project's planning rules,
those are DSL-declaration decisions and must not be inferred while
writing this task's contract — they need Rafael's explicit sign-off
first, the same way `nav_tabs`' four decisions were settled before its
task 1 was written.

## Proposed contract (draft, pending the decisions above)

When `app.yaml` declares `status:`, Stage 2 reserves a fixed
`STATUS_BAR_H` band as the true top of every screen (above the nav band
when `app.nav` is also set), and Stage 3b bakes a status descriptor
(text + rect) the runtime can draw from. No rendering in this task —
data + geometry only, mirroring `nav_tabs` task 1's split.

## Next step

Do not start implementation. Bring the open questions in `../epic.md`
back to Rafael; once settled, rewrite this file with a full contract
(In / Delivered / Not in scope / Dependencies / Pre-work / Tests / DoD,
same shape as the `nav_tabs` epic's task files) before cutting a task
branch for it.
