---
name: investigation-update-fields
description: Update an existing Investigation Plan in place after a dive session — dive statuses, driving-question checkboxes with resolution pointers, artifact links, immediate-action checkoffs, and the header Status. Use after a deep dive session, or whenever asked to update investigation status or progress. Not user-invocable directly — used internally by the /investigation-update command.
user-invocable: false
---

# investigation-update-fields

Modifies `docs/context/investigation-plan-<slug>.md` in place, against a short allow-list of what can change — everything else is off-limits.

## Allowed to touch

- Header `Status` field (e.g. `In Progress (2/7 deep dives concluded)` → `Concluded`)
- Per-dive `Status` fields (`Not Started` / `In Progress` / `Concluded`)
- Driving-question checkboxes — **only with a resolution pointer appended inline**: `— resolved (session log N)` or `— relocated to <destination> (session log N)`. A checked box with no pointer is an update error, not a style choice.
- **Artifacts** lines: adding links/paths once an artifact actually exists
- **Prior material** lines: appending newly-surfaced leanings, each with its source session log
- Immediate-action checkoffs, with the same resolution-pointer rule

## Never touch

- Driving-question wording, exit criteria, Method / Feeds-into / Depends-on fields
- §2 (the protocol), §4 (dependency map), §5 (mapping table structure)
- Adding, removing, splitting, or renumbering dives — that's authoring: route to `/investigation-new` in append mode
- Anything in a dive that hasn't been worked on

If something outside this list looks wrong (a typo, a stale path, a dependency that no longer makes sense), point it out to the user instead of silently fixing it — that's an edit, not a status update.

## After writing

Print a diff-style summary of what changed: field → old value → new value.

## Closing the loop

- If every dive feeding one §5 deliverable is now `Concluded`, tell the user that deliverable is startable (for a spec, that means `/spec-new` can run its interview without fabrication).
- If every dive is `Concluded`, set the header Status to `Concluded` and say the plan's exit condition is met — the investigation is over; the deliverable sequence is the work now.
