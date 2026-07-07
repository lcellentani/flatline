---
name: spec-review-checklist
description: Audit a SPEC for ambiguity, missing non-goals, unfalsifiable goals, or unresolved open questions before implementation planning starts. Use whenever a spec has just been drafted or edited and before it's declared ready, or whenever the user asks to review, check, or audit a spec. Not user-invocable directly — used internally by the /spec-review command.
user-invocable: false
---

# spec-review-checklist

A spec-level audit: read-only, returns a structured findings list, and never edits the spec itself — the same checkpoint discipline this workflow uses before any content gets relied on downstream.

## Context efficiency

If a subagent (Task tool) is available, delegate the audit to it: pass it the spec file and this checklist, and have it return only the structured findings list. Gap analysis tends to be verbose reasoning that doesn't need to live in the main session's context — only the conclusions do.

## Checklist

- **Ambiguous requirements** — any sentence in Goals or Design that could be read two different ways.
- **Goals aren't falsifiable** — "make it feel better" is not a goal; "input latency under 50ms" is.
- **Non-Goals are too thin or missing** — a spec with zero exclusions usually means scope wasn't actually thought through.
- **Design makes unstated assumptions** — about existing systems, performance budgets, or platform constraints that aren't written down.
- **Open Questions left unresolved without being flagged** — if the Design section quietly assumes an answer to something genuinely unsettled, that's a gap, not a decision.
- **Status mismatch** — a spec marked `Ready` that still has unchecked Open Questions.

## Output format

For each finding: `[Severity] Section — what's wrong — suggested fix.` Severity is one of `Blocking` (don't plan against this yet), `Should-fix` (fixable in five minutes, do it before planning), `Nice-to-have` (note it, don't block on it).

If there are no findings: say so plainly — `No gaps found — ready for /plan-draft.` Don't manufacture findings to seem thorough.
