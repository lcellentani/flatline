---
name: plan-review-checklist
description: Audit an Implementation Plan against engineering-principles and completeness checks before implementation starts. Use whenever a plan has just been drafted or edited, before starting work on it, or whenever the user asks to review, check, or approve a plan. Not user-invocable directly — used internally by the /plan-review command.
user-invocable: false
---

# plan-review-checklist

A plan-level audit: read-only, returns a structured findings list, and never edits the plan itself — the checkpoint between drafting and acting on it.

## Context efficiency

If a subagent (Task tool) is available, delegate the audit to it and have it return only the structured findings list — same reasoning as `spec-review`.

## Checklist

- **Every task has acceptance criteria** that are testable, not descriptive.
- **Every task has a delegation tag** — none left as `[T1 / T2 / T3 / T4]` placeholder text. If the tier is genuinely ambiguous, that itself is a finding.
- **Every phase has a rollback line.**
- **Files to Modify entries have an LOC estimate and a risk rating** — an empty or vague entry is a gap.
- **Scope is explicit** — In Scope / Out of Scope aren't left as inherited boilerplate from the spec without being checked against what the tasks actually do.
- **No scope drift from the spec** — if a task does something the spec's Non-Goals excluded, or introduces a goal the spec never stated, flag it. That's a signal to go back to `/spec-review` or amend the spec, not silently expand the plan.
- **No TODO/TBD left in the body.**
- **Phase/task numbering is consistent** if this is an appended plan (Phase 2 doesn't quietly restart at Task 1.1).

## Output format

Same as `spec-review`: `[Severity] Section — what's wrong — suggested fix`, severity `Blocking / Should-fix / Nice-to-have`. If clean: `Approved — ready to implement.`
