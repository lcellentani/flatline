# [Feature Name] — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/[feature-slug].md` |
| **Date** | [YYYY-MM-DD] |
| **Status** | Not Started |
| **Effort** | [rough estimate, e.g. "2–3 sessions"] |

---

## 1. Overview

### Goal

[1–3 sentences: what you'll be able to do after this ships. Pull this from the spec's Goals section — don't redefine it.]

### Approach

- [Key technical decision or pattern 1]
- [Key technical decision or pattern 2]
- [How existing infrastructure is reused]

### Scope

**In Scope:** [feature elements covered by this plan]
**Out of Scope:** [pulled from the spec's Non-Goals, plus anything deferred to a later phase]

---

## 2. Phased Task Breakdown

### Phase 1: [Name] — Not Started

**Objective:** [One sentence.]

#### Task 1.1: [Task Name]

**File:** `[path/to/file.ext]` [or **New File:**]
**Tag:** [T1 / T2 / T3 / T4] — [one-sentence rationale]

**Changes:**
- [Specific change 1]
- [Specific change 2]

**Acceptance Criteria:**
- [Testable assertion 1]
- [Testable assertion 2]

#### Task 1.2: [Task Name]

[Same structure as 1.1]

**Rollback:** [One line — usually "git revert the commit(s) for this phase."]

---

### Phase 2: [Name] — Not Started

[Same structure as Phase 1]

---

## 3. Files to Modify

| Path | Purpose | Est. LOC | Risk |
|------|---------|----------|------|
| `path/to/file.ext` | [What changes] | +N | Low/Medium/High |
| `path/to/new.ext` | **New** — [purpose] | ~N | Low/Medium/High |

---

## 4. Validation Checklist

### Functional
- [ ] [Primary scenario]
- [ ] [Edge case]

### Integration
- [ ] [Existing behavior unaffected]

### Regression
- [ ] [Existing feature still works]

---

## 5. Risk Summary

> Omit this section entirely if there's nothing non-trivial to flag.

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| [Risk] | Low/Medium/High | Low/Medium/High | [Specific countermeasure] |

---

## Status Legend

| Marker | Meaning |
|--------|---------|
| Not Started | Work hasn't begun |
| In Progress | Currently being implemented |
| Done | Phase implemented and verified |

Update the header `Status` field as phases complete (e.g. `In Progress (2/4 phases done)`). `/plan-update` does this automatically.
