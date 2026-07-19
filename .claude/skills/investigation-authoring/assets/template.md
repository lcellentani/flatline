# [Problem Space Name] — Investigation Plan

| Field | Value |
|-------|-------|
| **Status** | In Progress (0/[N] deep dives concluded) |
| **Date** | [YYYY-MM-DD] |
| **Scope** | [One sentence: the problem space this investigation must bring to clarity, and what that clarity unblocks] |
| **Context** | [Links: vision/context docs, the specs/systems this builds on, the session log recording the founding brainstorm] |

---

## 1. Purpose & exit condition

[1–2 sentences: why this problem space needs an investigation program rather than a single spec interview.]

This document is process-context, not an implementation plan — plans derive from Ready specs, and this document's purpose is to *produce* the downstream deliverables.

**The contract, in one line:** each deep dive closes a named set of driving questions and emits artifacts; when every dive feeding a given deliverable is `Concluded`, that deliverable can be started without fabricating a single goal or design decision.

**Exit condition:** the plan is done when [the named downstream deliverables — a spec sequence, a go/no-go decision, an architecture doc…] are *startable*, not when they are written.

**External gates:** [Any go/no-go dependency that gates part of the program — an experiment that must pass, a budget call, an upstream team. Name which dives it gates, and why the ungated dives don't poison it. Delete this block if there are none.]

## 2. How a deep dive runs (Human~AI protocol)

- **The human brings:** taste calls, first-hand evidence, access to [hardware / data / stakeholders], and final say on every lock.
- **The agent brings:** drafts of real artifacts, evidence audits, throwaway spike code, and option analyses that always end in a recommendation — never an unranked survey.
- **Cadence:** each dive session appends a numbered session log. Every session ends with an explicit split: *decisions locked* vs. *still open*. A decision made in conversation but not written down does not exist.
- **Conclusion rule:** a dive is `Concluded` only when every driving question is checked or explicitly relocated (naming its destination). Exit criteria are falsifiable on purpose — a dive that can't say what "done" looks like isn't a dive, it's musing.
- **Prior material is leanings, not decisions.** A dive re-examines its leanings before locking them; it does not treat them as already decided, and it does not re-derive them from scratch either.

## 3. Deep dive roster

[Repeat the block below per dive. Methods come from a closed set: **evidence audit** (mine existing data/logs/docs), **design brainstorm** (converge on a design through discussion plus drafted artifacts), **code spike** (throwaway code — the artifact is the numbers/answers, not the code), **paper walkthrough** (trace one concrete scenario end-to-end by hand).]

### DD-1 — [Title]

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | [evidence audit / design brainstorm / code spike / paper walkthrough] |
| **Feeds into** | [deliverable, or a downstream dive] |
| **Depends on** | [dives / external gates / "Nothing — start immediately"] |

[Optional context paragraph: the framing this dive starts from, if the founding brainstorm produced one. Keep it a framing, not a conclusion.]

**Driving questions**
- [ ] [A checkable question — someone can point at an artifact or logged decision and say "answered." Include the known fork/options inline if the brainstorm surfaced them, with any leaning marked as a leaning.]
- [ ] [Question 2]

**Prior material:** [links to session logs / brainstorm conclusions this dive starts from, explicitly labeled as leanings]

**Artifacts:** [named outputs with their landing paths]

**Exit criteria:** we have clarity when [one falsifiable sentence].

## 4. Dependency map

[A small diagram or list: which dives start immediately (run empirical spikes in parallel with judgment dives), which chain, which are gated and on what. A map where everything depends on everything is a roster problem — go split dives instead.]

## 5. Deep dive → deliverable mapping

| Deep dive | Produces the material for | Notes |
|---|---|---|
| DD-1 | [deliverable] | [note] |

[If the deliverables have an authoring order, state it here.]

## 6. Immediate actions (before any dive concludes)

[Small, concrete actions worth doing now — usually data-capture changes that make future evidence better, or cheap fixes that make every later session more valuable. Each one must first be checked against prior recorded decisions; if it touches one, surface the tension honestly instead of silently overriding it. Delete this section if there are none.]

1. [Action — why now, and what it unblocks]

---

## Status Legend

| Status | Meaning |
|--------|---------|
| Not Started | Dive not yet begun |
| In Progress | At least one session logged against it |
| Concluded | Every driving question checked or explicitly relocated; artifacts exist |
