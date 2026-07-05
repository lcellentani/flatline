# Session Log 11 — GM Adjudicator: Same-Batch Effect Conflicts

| Field | Value |
|---|---|
| **Date** | 2026-07-03 |
| **Scope** | Close `gm-adjudicator.md` §5's last open question end to end (brainstorm → lock → implement → verify): same-batch effect conflicts. This is the item session log 10 left as "the one substantive open question left in the document" — closing it brings the spec to Ready. |
| **Follows** | `session-log-10-relocate-deferred-open-questions.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_gauntlet.hpp`, `gm_applier.hpp`, `gm_log.hpp`, `gm_rules.hpp`, `wizard_of_oz_demo.cpp`, `test_gauntlet.cpp`, `test_log.cpp` |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

No new headers this session — every change is a modification to an already-built artifact.

Modified artifacts:
- `gm_gauntlet.hpp` — `run_gauntlet` now folds state forward across a branch (a `history` vector of intermediate `World` snapshots) instead of checking every effect against the same static pre-turn `world`. New `Stage::BatchConflict` enum value and `stage_name` mapping. `Failure` gained an optional `conflicting_with_index` field, populated only for `BatchConflict`. `ids_exist` deliberately stays checked against the untouched `world`, never the folded state. `modify_disposition`'s clamp (`clamp_disposition_delta`) now reads the folded state instead of the static snapshot. Added `#include "gm_applier.hpp"` to reuse `applier::detail::apply_one` for building the scratch/history states, rather than duplicating per-effect-type mutation logic a second time.
- `gm_log.hpp` — `GauntletRejected` gained an optional `conflicting_with_index` field, mirroring `roll`'s existing omitted-not-null idiom in both `to_json`/`from_json`; tolerant of its absence on log lines written before this field existed.
- `wizard_of_oz_demo.cpp` — the one `log::GauntletRejected{...}` construction site now threads `f.conflicting_with_index` through.
- `test_gauntlet.cpp` — five new tests: transfer_item duplication (the traced bug from session log 10) now caught as `BatchConflict` naming the culprit; gold double-spend (the other traced bug) likewise; an ordinary never-legal precondition failure confirmed to stay `Stage::Preconditions` with the field absent (regression guard); an item consumed earlier in the batch and re-referenced confirmed to be a `BatchConflict`, not a hallucination (exercises the `ids_exist`-stays-static decision directly); `modify_disposition` confirmed to compound correctly at the `[Hostile, Allied]` ceiling across a batch instead of both effects clamping from the same starting value.
- `test_log.cpp` — four new round-trip checks for `GauntletRejected.conflicting_with_index`: absent on an ordinary rejection, present and round-trips as a real value on a batch conflict, and a pre-existing log line without the field still parses with it defaulting to absent.
- `gm-adjudicator.md` — new Key Decision locking fold-forward preconditions + `BatchConflict`; the Acceptance Gauntlet section's per-effect and apply-atomically paragraphs revised to describe the fold (a revision of the previously-locked "checked against a single pre-turn snapshot" line, named as a revision, not a silent extension); a fifth worked example (same-batch conflict, transfer_item duplication); **"Same-batch effect conflicts" open question resolved and closed**; **Status flipped from Draft to Ready** — this was the last unresolved item in §5.

## 2. Decisions locked this session

### 2.1 The brainstorm, compressed

Four options were on the table from session log 10, plus two more surfaced in conversation this session:

1. Batch-level pre-check (a new gauntlet stage scanning for same-resource claims before anything runs).
2. Sequential / fold-forward validation (each effect checked against the running state left by prior effects in the branch).
3. Authoring-convention-only (leave unenforced, communicate the rule to whoever fills out the work order).
4. Narrow patch (fix just the two traced bugs, no general framework).
5. A `NeedsResolution` outcome with interactive mid-turn resolution ("pull-push-resolve" — surfaced by Ludo, explored and narrowed).
6. **Locked:** fold-forward validation (2), whole-batch rejection with a structured diagnostic naming the colliding effect — the "option (a)" version of (5), not the interactive version.

### 2.2 Why (5) narrowed the way it did

The interactive/partial-application version of `NeedsResolution` was weighed against two already-locked decisions and lost on both:
- **Atomicity.** Partial application (apply the non-conflicting effects, hold the conflicting one pending) directly contradicts "apply atomically... no partial application" (locked, session log 5). The all-or-nothing version doesn't touch this.
- **One call, not two.** A synchronous resolve step is a second inference dependency per turn on the conflict path — free for Experiment 1's human GM, but exactly the round-trip cost session log 1 §2.7 rejected for Experiment 2, just relocated from authoring time to validation time. Confirmed this would resurrect the already-relocated **Failure policy (Experiment 2)** question (session log 10) rather than staying inside this spec's scope.

Landed on: whole-batch rejection stays exactly as already locked; the only change is a richer, structured `Failure` (naming the colliding effect index) instead of a plain string, so a resubmission — however it happens, on whatever cadence — doesn't have to guess.

### 2.3 Why (3) was ruled out plainly

Authoring-convention-only has direct prior art against it in this project: already rejected once for `modify_resource(hp)` on the grounds that unenforced is invisible in Experiment 1 and meaningless in Experiment 2. Both halves transfer directly, and Experiment 2's half is sharper here — the closed-vocabulary composition bet ("map novel actions onto existing primitives") is exactly what makes same-batch collisions likely for a model, not just a human's rare slip. Kept as a complementary, not substitutive, addition: worth documenting in authoring guidance regardless of the validator mechanism, to reduce how often the validator has to fire at all.

### 2.4 Why (1) (batch-level pre-check) lost to (2) (fold-forward) even at equal implementation cost

Found while implementing: `clamp_disposition_delta` had the identical bug shape as the two traced item/gold bugs — two `ModifyDisposition` effects on the same entity in one batch both clamped from the same static pre-turn disposition value instead of the second compounding on the first's result, silently overshooting the `[Hostile, Allied]` ceiling once both deltas were summed at apply time. A batch-level pre-check scoped to "same-resource claims" (built for the two known bugs) would not have caught this on its own — it's a different shape (compounding correctly, not rejecting) that the general fold-forward mechanism catches for free, because it's the same underlying fix (check against the running state, not the snapshot) applied uniformly. This is the concrete case for choosing the general mechanism over the narrower, cheaper options: same implementation cost, strictly wider coverage, proven by something no one had traced or asked for going in.

### 2.5 Implementation shape, as built

- `run_gauntlet` builds a `history` vector of `World` snapshots (`history[0]` = untouched pre-turn `world`; `history[k+1]` = state after folding in `applied[k]`), purely as validation-internal bookkeeping — discarded either way, never touching the real `apply()` call downstream.
- `ids_exist` stays checked against `world`, never `history`/folded state. Traced precisely to `item_exists` (`gm_rules.hpp`): it proves an id is real *somewhere in current holder state*, not against a persistent registry, so an item a prior effect already consumed to `VOID_ENTITY` would otherwise wrongly read as a hallucination instead of the same-batch conflict it actually is. No effect in the v0 vocabulary creates or destroys an id, so pinning existence to the pre-turn snapshot is never stale.
- `preconditions` checks the folded state. On failure, a second check against the untouched `world` distinguishes "never legal, even at turn start" (ordinary `Stage::Preconditions`) from "legal at turn start, broken by this batch" (`Stage::BatchConflict`). On the latter, `history` is replayed to find the earliest index whose application first broke the same precondition — `Failure::conflicting_with_index`.
- Reused `applier::detail::apply_one` (via a new `#include "gm_applier.hpp"`) to build each folded step, rather than writing a second, parallel "how does each effect type mutate `World`" — the same "one computation, not two independently-maintained copies" discipline `modifier_magnitude_sum` already established (session logs 4/7). No circular include: `gm_applier.hpp` has no dependency on `gm_gauntlet.hpp`.
- `Failure::conflicting_with_index` is a single optional field, not a variant restructuring — one case doesn't clear the bar set by "hardcoded values promoted to data-driven structures only when a second use case makes the single value hurt." Revisit if a second stage ever needs the same shape.

## 3. Verification performed

- Baseline, before any change: recompiled and reran all four suites — `test_gauntlet` 17/17, `test_applier` 31/31, `test_roll` 20/20, `test_log` 33/33, demo exit 0 — confirmed session log 9/10's state reproduced exactly.
- After `gm_gauntlet.hpp`: `-fsyntax-only` clean (only the pre-existing benign `-Wmissing-field-initializers` noise); `test_gauntlet` rebuilt and rerun against the unmodified test file first — all 17 original cases still pass unchanged, confirming the fold-forward change is behavior-preserving on every previously-covered case before any new coverage was added.
- Added the five new `test_gauntlet.cpp` cases: **22/22.**
- After `gm_log.hpp` + `wizard_of_oz_demo.cpp` + the four new `test_log.cpp` cases: full rebuild of all four suites and the demo. **`test_gauntlet` 22/22, `test_applier` 31/31 (untouched, unchanged), `test_roll` 20/20 (untouched, unchanged), `test_log` 37/37, demo exit 0** — demo's logged output identical to session log 9/10's run (the fixed single demo turn never exercises a batch conflict).
- Confirmed by inspection that `apply()` (`gm_applier.hpp`) required zero changes — the fold-forward `history` is entirely internal to `run_gauntlet`, discarded on both the success and failure paths; the real apply-time World and the vital-state derivation (`vital_deltas`, session log 9) are untouched.

## 4. Open questions status (`gm-adjudicator.md` §5)

- [x] Validator reference data — resolved (session log 4).
- [x] hp floor — resolved (session log 6).
- [x] hp ceiling and alive/dead state — resolved (session log 9).
- [x] **Same-batch effect conflicts — resolved this session.**
- [x] `narrate` vs. turn-level narration — resolved (session log 9).
- [x] Failure policy — relocated (session log 10).
- [x] Trigger boundary — relocated (session log 10).
- [x] Unvalidated branch hides Experiment 2 failures — relocated (session log 10).

**Every item in §5 is now `[x]`. `gm-adjudicator.md`'s Status field flipped Draft → Ready this session.**

## 5. Noticed, not touched this session

`gm-adjudicator.md` §3 (Non-Goals) still reads "v0 locks a *representative* 7-primitive starter set" — stale since session log 9 dropped `narrate` to a 6-primitive vocabulary (§4's own Key Decisions bullet already says 6). Unrelated to this session's scope; flagging rather than silently folding an unrelated fix into this diff.

## 6. Suggested next step

The spec is Ready. Per the project's own status legend, next is `/plan-draft` — or, matching what's actually been named as the real gap on the horizon (memory, "on the horizon" section): the genuine multi-turn playable loop with live WorkOrder authoring and real turn sequencing, since that's what produces actual Experiment 1 session data and the "is it fun" signal nothing built so far has generated. `wizard_of_oz_demo.cpp` still runs exactly one hardcoded turn.
