---
name: spec-authoring
description: Draft a new SPEC design document for a feature — problem statement, goals, non-goals, design approach, and open questions — before any implementation planning begins. Use this whenever the user wants to start a new feature, asks to "write a spec" or "design doc," or describes a feature idea that hasn't been scoped yet, even if they don't use the word "spec." Always run this before drafting an implementation plan for any feature with no existing SPEC file. Not user-invocable directly — used internally by the /spec-new command.
user-invocable: false
---

# spec-authoring

Produces a SPEC at `docs/specs/<feature-slug>.md`, following the structure in `assets/template.md` (bundled with this skill).

## Process

1. **Slug the feature name.** Lowercase, hyphenated (e.g. `ghost-chase-ai`). This is the filename and the cross-reference key for the matching plan.
2. **Check for an existing spec at that slug.** If one exists and is `Draft` or `Ready`, ask whether this is an edit or a genuinely new spec (use `Supersedes` if it's a replacement).
3. **Interview before writing.** Don't fabricate goals or design decisions. If the conversation doesn't already contain answers, ask:
   - What's the problem? What's missing or broken today?
   - What does success look like — what should be observably true afterward?
   - What's explicitly out of scope, and why?
   - What existing systems (ECS components, render passes, input bindings, etc.) does this touch or build on?
   - Anything still undecided?
4. **Write the SPEC** using `assets/template.md`. Keep the Design section about *why*, not file-by-file *how* — that's `plan-draft`'s job, not this one.
5. **Surface, don't resolve, open questions.** If something is genuinely unresolved, it goes in Open Questions, not a quiet assumption baked into the Design section.
6. **Set Status to `Draft`.** Only the user moves it to `Ready` — that's a judgment call, not something to auto-set.

## What this is not

- Not a task breakdown. No file lists, no LOC estimates, no phases — those live in the plan.
- Not a place to assign delegation tags — those are per-task, in the plan.

## After writing

Tell the user the file path, and suggest `/spec-review` before moving on to `/plan-draft` if the spec has any non-trivial open questions or this is a larger feature.
