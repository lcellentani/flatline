---
name: plan-update-fields
description: Update an existing Implementation Plan in place after a work session — phase/task status markers, Actual delegation tags, and the header Status field. Use after finishing implementation work on a task or phase, or whenever asked to update plan status, progress, or mark something done. Not user-invocable directly — used internally by the /plan-update command.
user-invocable: false
---

# plan-update-fields

Modifies `docs/plans/<feature-slug>.md` in place, against a short allow-list of what can change — everything else is off-limits.

## Allowed to touch

- Header `Status` field (e.g. `Not Started` → `In Progress (2/4 phases done)` → `Done`)
- Phase status markers (`Not Started` / `In Progress` / `Done`)
- Per-task **Actual tag**, if the real engagement differed from the planned `Tag` — add it as a second line under the task, don't overwrite the original: `**Actual:** T3 — [why it changed]`
- Task-level checkmarks if the template uses them

## Never touch

- Goal, Approach, Scope
- Acceptance Criteria wording
- Files to Modify table contents
- The Spec link in the header
- Anything in a phase that hasn't been worked on yet

If something outside this list looks wrong (a typo, a stale file path), point it out to the user instead of silently fixing it — that's an edit, not a status update.

## After writing

Print a diff-style summary of what changed: field → old value → new value.

## Closing the loop

If this update brings every phase in the plan to `Done`, ask the user whether `docs/specs/<feature-slug>.md` should move from `Draft`/`Ready` to `Implemented`. Don't change the spec's status yourself — that's the user's call — but don't let a fully-implemented feature sit under a stale spec status either.
