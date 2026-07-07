# [Feature Name] — SPEC

| Field | Value |
|-------|-------|
| **Status** | Draft |
| **Date** | [YYYY-MM-DD] |
| **Plan** | [link to docs/plans/<feature-slug>.md once it exists, otherwise "Not yet planned"] |
| **Supersedes** | [link to prior spec this replaces, or "N/A"] |

---

## 1. Problem

[1–3 sentences: what's missing, broken, or limiting right now. State the problem, not the solution.]

## 2. Goals

[Observable outcomes that define success. Each goal should be falsifiable — something you can point at and say "yes, this happened" or "no, it didn't."]

- [Goal 1]
- [Goal 2]

## 3. Non-Goals

[Explicitly excluded scope, with a one-clause reason for each. This is what keeps the implementation plan from drifting.]

- [Excluded item 1] — [why]
- [Excluded item 2] — [why]

## 4. Design

[How this fits the existing architecture. Name the key decisions and, briefly, the alternative(s) you considered and rejected — one line each is enough. This section is about *why*, not file-by-file *how*; that belongs in the implementation plan.]

### Key Decisions

- [Decision 1] — [one-line rationale; alternative considered, if any]
- [Decision 2] — [one-line rationale]

### How It Fits Existing Systems

[Which existing modules/patterns this touches or reuses, e.g. ECS components, render passes, input bindings.]

## 5. Open Questions

[Anything genuinely unresolved. Don't silently resolve these by assumption — flag them here so `/spec-review` and `/plan-draft` both see them.]

- [ ] [Open question 1]
- [ ] [Open question 2]

---

## Status Legend

| Status | Meaning |
|--------|---------|
| Draft | Still being shaped; open questions may exist |
| Ready | Open questions resolved; safe to run `/plan-draft` |
| Implemented | All linked plan phases are Done |
| Superseded | Replaced by a newer spec — link it above |
