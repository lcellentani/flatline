# Session Log 10 — GM Adjudicator: Relocating Deferred Open Questions

| Field | Value |
|---|---|
| **Date** | 2026-07-03 |
| **Scope** | Administrative only: formally relocate three of `gm-adjudicator.md` §5's open questions (failure policy, trigger boundary, unvalidated-branch) that were verbally agreed to move out of this spec in session log 9 §4, but never actually written into the spec text. No brainstorm, no code — the decision was already made; this closes the gap between decision and document. |
| **Follows** | `session-log-9-narrate-removal-and-vital-state.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec) |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

No code changes — spec-text only.

Modified artifact:
- `gm-adjudicator.md` — §5's three infrastructure-dependent open questions marked `[x]` with explicit "relocated, not resolved here" language, each pointing to `/spec-new` as the mechanism for properly scoping them once their prerequisite infrastructure (an Experiment-2 harness or a movement system) actually exists. Non-Goals, Key Decisions, worked examples, and every other section left untouched.

## 2. Decision applied this session (administrative, not brainstormed)

No brainstorm — the decision itself was already locked in session log 9 §4: *"agreed in an earlier part of this session's conversation to relocate out of this spec entirely, into a future Experiment-2/movement spec... Not yet applied to the spec text."* This session applies it:

- **Failure policy** (Experiment 2) — relocated.
- **Trigger boundary** (deferred) — relocated.
- **Unvalidated branch hides Experiment 2 failures** — relocated.

Deliberately left unresolved as part of this relocation, not an oversight: whether these three land in one combined future spec or split across a dedicated Experiment-2-harness spec and a separate movement spec. That's a real scoping call for a future `/spec-new` interview, not something to decide as a side effect of a cleanup pass — `/spec-new`'s own discipline ("interview before writing, don't fabricate goals or design decisions") rules out inventing that structure now.

## 3. Verification performed

Text-only edit — no build, no test suite affected. Confirmed by inspection that none of the three relocated items recur elsewhere in the document (Key Decisions, Non-Goals, worked examples never referenced them individually, so nothing else needed to change).

## 4. Open questions status (`gm-adjudicator.md` §5)

- [x] **Validator reference data** — resolved (session log 4).
- [x] **hp floor** — resolved (session log 6).
- [x] **hp ceiling and alive/dead state** — resolved (session log 9).
- [ ] **Same-batch effect conflicts** — untouched. The one substantive open question left in the document.
- [x] **`narrate` vs. turn-level narration** — resolved (session log 9).
- [x] **Failure policy** — relocated this session, not resolved.
- [x] **Trigger boundary** — relocated this session, not resolved.
- [x] **Unvalidated branch hides Experiment 2 failures** — relocated this session, not resolved.

Visible open-question count against this document: **1**.

## 5. Suggested next step

**Same-batch effect conflicts** — the full brainstorm → lock → implement → verify treatment, same weight session log 9 gave `narrate`/vital-state. This is what gets `gm-adjudicator.md` to Ready.

Two confirmed bugs already traced against the current code (not hypothetical — reachable in v0 as it stands today), ready to ground the brainstorm and to serve as test cases for whatever gets locked:

- **`transfer_item` duplication.** Two effects moving the same item to two different targets both independently pass `preconditions`'s `holds()` check against the same static pre-turn snapshot. Applied sequentially, the item ends up in *both* destinations' inventories — a real duplication bug.
- **`modify_resource(gold)` double-spend.** Two negative-gold effects each independently pass `gold_spend_legal()` against the pre-turn balance. Applied sequentially, the player's gold can go negative — directly violating the already-locked "gold delta rejected if negative" decision (§4 Key Decisions).

Four options already named, not yet weighed against each other:

1. **Batch-level pre-check** — a new gauntlet stage (parallel to `validate_modifiers`) that scans a branch for effects claiming the same mutually-exclusive resource and rejects the whole batch atomically before anything runs.
2. **Sequential / fold-forward validation** — each effect in a branch is checked against the running state left by the effects before it, not the static pre-turn `world`. Fixes both bugs precisely; structural change to `run_gauntlet`.
3. **Authoring-convention-only** — leave unenforced. Weak precedent: already rejected once for `modify_resource(hp)` on the grounds that unenforced is invisible in Experiment 1 and meaningless in Experiment 2.
4. **Narrow patch** — fix just the two traced bugs directly, no general batch-conflict framework. Cheap, but symptom-level; doesn't generalize to a conflict type not yet hit.
