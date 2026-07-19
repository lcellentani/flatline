---
name: investigation-authoring
description: Draft a new Investigation Plan for a large, uncertain problem space — a program of deep dives with driving questions, artifacts, and falsifiable exit criteria, ending when downstream deliverables (specs, go/no-go decisions, architecture docs) become startable. Use when a problem space is too large or unknown for a single spec interview — the tell is "we don't yet know what the specs even are" — or when the user asks to plan an investigation, exploration program, or research roadmap. Not user-invocable directly — used internally by the /investigation-new command.
user-invocable: false
---

# investigation-authoring

Produces `docs/context/investigation-plan-<slug>.md`, following the structure in `assets/template.md` (bundled with this skill). An Investigation Plan is process-context, not an implementation plan — plans derive from Ready specs; an investigation exists to *produce* the specs (or other deliverables). Its exit condition is always "the downstream deliverables are startable," never "they are written."

## When this applies (vs. `/spec-new`)

If the user can already name the single contract to design, this is a spec — hand off to `/spec-new`. Use this skill when the problem space would need several specs, an ordering between them, and evidence that doesn't exist yet.

## Process

1. **Slug the problem space.** Lowercase, hyphenated (e.g. `experiment-2`). Filename: `investigation-plan-<slug>.md`.
2. **Check for an existing plan at that slug.** If one exists, this is **append mode**: propose new/changed dives against the existing roster, never renumber or rewrite concluded dives. Status-only changes are not authoring — route them to `/investigation-update`.
3. **Phase 0 — Intake interview.** Interview before writing; never fabricate goals, gates, leanings, or roster structure. Cover:
   - What does "clarity" unblock — what are the candidate downstream deliverables?
   - What evidence already exists, and where? What has already been *decided* vs. merely *leaned*?
   - Are there external gates (an experiment that must pass first, a budget call, an upstream dependency)? Which parts of the program do they actually gate?
   - What prior recorded decisions could conflict with obvious actions?

   For the last two, **go verify in the repo — don't just ask.** Read the relevant specs, plans, and session logs; a prior decision the human forgot is exactly the kind this step exists to catch.
4. **Phase 1 — Roster proposal, then hard lock.** Propose the dive roster at high altitude first (per dive: title, 2–3 line description, key questions, method, artifact, feeds-into) plus the points needing the human's call, and **stop for an explicit lock. Never draft the full document before the roster is approved.** Roster heuristics:
   - One question-cluster/contract per dive — split mega-dives.
   - Methods come from the closed set in the template: evidence audit, design brainstorm, code spike, paper walkthrough.
   - Identify which dives are immediately startable; run empirical dives (spikes) in parallel with judgment dives.
   - Gate-dependent dives go last; every dive names what it feeds.
5. **Phase 2 — Full draft** against `assets/template.md`. Rules:
   - Every driving question is checkable — someone can point at an artifact or logged decision and say "answered."
   - Every exit criterion is one falsifiable sentence.
   - Conversation material is captured as **leanings assigned to a dive** (in its Prior material), never as decisions.
   - Immediate actions require a prior-decision conflict check; if one touches a recorded decision, surface the tension honestly in the text rather than silently overriding it.
6. **Companion session log — mandatory.** In the same pass, append a numbered session log (`docs/context/session-log-<N>-<topic>.md`, N = highest existing + 1) using the session-log template bundled with the `investigation-dive-session` skill. Split of labor: the plan holds the *program*; the log holds the *reasoning trail* (why the roster is shaped this way, what was leaned, what was deliberately not decided).
7. **Set header Status to `In Progress (0/N deep dives concluded)`.**

## What this is not

- Not the place to decide deliverable *content* — a plan that quietly answers a spec's design questions has jumped its own gate.
- Not an implementation plan: no phases, no files-to-modify, no delegation tags.

## After writing

Tell the user both file paths and which dives are immediately startable, and suggest `/investigation-dive <slug> <dive-id>` for the first one.
