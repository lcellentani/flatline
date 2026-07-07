---
name: plan-drafting
description: Draft or extend a lean Implementation Plan from an approved SPEC — phased task breakdown, files-to-modify, acceptance criteria, and a per-task delegation tag. Use whenever asked to draft an implementation plan, break a spec into tasks, plan out a feature, or continue planning the next phase of an existing plan — even without the exact phrase "implementation plan." Not user-invocable directly — used internally by the /plan-draft command.
user-invocable: false
---

# plan-drafting

Produces or extends `docs/plans/<feature-slug>.md`, following the structure in `assets/template.md` (bundled with this skill).

## Preconditions

- A matching spec must exist at `docs/specs/<feature-slug>.md`. If it doesn't, stop and tell the user to run `/spec-new` first — don't draft a plan against an assumed spec.
- Pull Goal, Scope, and Approach context from the spec rather than re-deriving them. The plan should never contradict its spec.

## Research before drafting

Don't write the Files to Modify table or LOC estimates from memory or assumption — ground them in the actual current codebase. If a subagent (Task tool) is available, delegate the exploration: have it locate the files the spec's "How It Fits Existing Systems" section points at, confirm current line counts/structure, and check for existing patterns to reuse, then return only the findings. Same reasoning as the `spec-review`/`plan-review` context-efficiency note — exploration is verbose, the main session only needs the result.

This is deliberately not run via Claude Code's built-in Plan Mode. Plan Mode is read-only end to end and would block the file write this skill ends with; it's also a separate, ephemeral artifact (`~/.claude/plans/`) from the durable plan this skill produces. Don't conflate the two — if the user says "enter plan mode," that's the permission-mode toggle, not this skill.

## Engineering Principles

These govern every decision in the plan:

- **Minimal footprint** — fewest files touched, fewest LOC. Reuse existing infrastructure (ECS components, helpers, patterns already in the codebase) rather than reinventing it. No speculative abstraction for a pattern used once.
- **Safe implementation** — new behavior should be invisible until explicitly triggered. No breaking changes to existing workflows when the new feature is inactive.
- **Least code changes** — if a refactor "would be nice" but isn't required, exclude it and note it as future work instead of folding it in.
- **Every task has acceptance criteria** — testable assertions, not vague descriptions.
- **Every phase has a one-line rollback** — for solo work this is almost always "git revert the phase's commit(s)"; only write something more specific if that's not true.

## Per-task delegation tag

One line, not a section. Use the project's tier vocabulary exactly:

- `T1` — written independently
- `T2` — written with AI answering questions
- `T3` — AI-drafted, author owns every line
- `T4` — fully delegated

Format: `**Tag:** T2 — [one-sentence rationale tied to *why this task*, not a generic justification].` If the right tier is unclear, default to the higher-ownership tier (T1/T2) and say so in the rationale rather than leaving it undecided. If a task mixes a real design decision with mechanical work, split the task — don't bury a T1 decision inside a T4 task.

## Appending vs. overwriting

If `docs/plans/<feature-slug>.md` already exists with completed phases, **append new phases** — don't overwrite or renumber what's done. The plan is a running record of the feature, not a single-shot document. This is what makes the repeat loop (`plan-draft` → execute → `plan-update` → `plan-draft` again) work without losing history.

## What to skip

- No reviewer sign-off and no external change-tracking system — git history is the record of who changed what, when, and why.
- No re-explaining the delegation tiers' definitions per plan — they're defined once, above; every task only needs the one-line tag.
- Risk Summary section: omit entirely if there's nothing non-trivial. Don't pad it to look thorough.

## After writing

Tell the user the file path and phase count, and suggest `/plan-review` before starting implementation.
