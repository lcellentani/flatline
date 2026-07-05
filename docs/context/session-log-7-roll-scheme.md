# Session Log 7 — GM Adjudicator: Roll Scheme & Seeded Roll

| Field | Value |
|---|---|
| **Date** | 2026-07-02 |
| **Scope** | Lock the spec's last open contract question ("modifier -> roll scheme"); build the seeded roll and wire it into the demo for real. |
| **Follows** | `session-log-6-applier.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_effects.hpp`, `gm_rules.hpp`, `gm_gauntlet.hpp`, `gm_roll.hpp` (new), `wizard_of_oz_demo.cpp`, `test_roll.cpp` |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

New artifacts:
- `gm_roll.hpp` — the seeded roll. `fnv1a_64(string) -> uint64_t`, `seed_for_turn(turn_number, work_order) -> uint64_t`, `RollResult{die, modifier_sum, total, success}`, `resolve(seed, modifier_sum) -> RollResult` (pure, independently testable), `resolve_turn(turn_number, work_order) -> RollResult` (convenience wrapper).
- `test_roll.cpp` — verification harness for this session's change, not a shipped project artifact. 20 checks: hash determinism, roll purity and die-range, the actual replay guarantee (two independently-constructed, content-identical `WorkOrder`s at the same `turn_number` produce an identical roll), turn-number and content sensitivity of the seed, and the modifier budget in both directions plus the exact boundary.

Modified artifacts:
- `gm_rules.hpp` — added `kDifficultyClass = 15`, `kModifierBudget = 5`, `modifier_budget_legal()`.
- `gm_gauntlet.hpp` — added `validate_modifiers()`, a new **WorkOrder-level** gauntlet stage, and its own `ModifierBudgetFailure` type (deliberately not folded into the existing per-effect `Failure`, since `Failure::effect_index` has no meaning before a branch is selected).
- `wizard_of_oz_demo.cpp` — `[ROLL]` is no longer a stand-in. The turn now runs modifier-budget check → roll → branch selection → gauntlet → apply, replacing the "always validate/apply `effects_on_success`" placeholder in place since session log 4.

## 2. Decisions locked this session

### 2.1 Roll scheme: Approach A (d20 + modifiers vs. a fixed DC)
Weighed against Approach B (modifier sum → probability curve, seeded uniform draw). Locked A for legibility — "sum +5 needs a 10+" is readable without running code, and matches language already in the spec ("the d20 stays a real d20"). B's structural edge (the probability clamp bounds outcomes for free, however large the modifiers get) was judged less load-bearing once the modifier budget (§2.2) closes the same gap a different way.

**Considered and rejected: summing multiple d20s** to widen the usable numeric range. Multiple dice trade a flat, uniform distribution for a triangular one — a modifier point buys less swing near the middle and more near the tails, turning the current hard, provable ceiling into a fuzzy "effectively certain past some point" one. That reintroduces the exact legibility cost that ruled out Approach B in the first place. Parked as a future option (e.g. advantage/disadvantage-style texture later), not needed for v0.

### 2.2 `kModifierBudget = 5`, derived FROM `kDifficultyClass = 15`, not picked independently
At DC 15 on a single d20, 5 is the largest budget that keeps both directions strictly probabilistic: best case needs a 10+ (55%), worst case needs a 20 (5%). One point higher and the best case would need less than 1 — every roll succeeds regardless of the die, silently reopening "the GM guarantees the outcome," the same failure shape as `Damage.via`'s original silent-bypass risk, just relocated from a single field to a sum.

Three sub-decisions locked alongside the number:
- **Enforcement:** gauntlet-enforced (`validate_modifiers`), not an authoring convention — unenforced is invisible in Experiment 1 (the human self-regulates) but meaningless in Experiment 2 (nothing stops a 4B model from ignoring unenforced prose).
- **Spend policy:** `<=`, not exact — a GM with one honestly-justified modifier shouldn't be forced to invent a second one just to max out the pool.
- **Aggregation:** bounded by **magnitude** (`sum(|value|)`), not net sum — a net-sum-only cap would still let a GM stack unbounded negative modifiers to force a guaranteed failure without ever tripping a positive-side limit.

Checked against `bribe_the_guard`'s existing modifiers (`+2, +3` → magnitude 5) — sits exactly at the cap, no fixture changes needed.

### 2.3 DC (`kDifficultyClass`) is a single fixed constant, not WorkOrder-authored
Rejected letting the GM set its own target per WorkOrder — that lets the GM guarantee an outcome without ever touching the die (`DC=1` ⇒ certain success, `DC=25` ⇒ certain failure), the same failure mode "engine owns the dice" already rules out for direct success/failure reporting, just relocated from the die to the goalpost. Discussed the correct locus IF difficulty ever needs to vary by action: a `DifficultyTier` enum (routine/contested/dangerous) mapped through a closed table to a DC — same shape as `kDamageCap` being per-item rather than a free field on `Damage`. **Parked, not built** — a single room doesn't need it yet.

### 2.4 Seed sourcing: `hash(turn_number, work_order_content)`
`turn_number` is engine/harness-tracked, not a `WorkOrder` field — sequencing is the caller's job, keeping the contract scoped to one turn at a time. Hash function: **FNV-1a, 64-bit**, not `std::hash` — `std::hash<T>` is unspecified across compilers and standard library versions (notably for `std::string` on some implementations), which would silently break the one property this whole scheme exists for: replaying a logged run identically later, possibly on a different machine. "Work order content" = the full canonical JSON dump (`gm_json.hpp`'s existing `dump`), prose fields included — chosen for simplicity over curating a mechanical-fields-only subset. Named as a real behavioral consequence, not a technicality: two GMs describing the identical bribe with different wording will roll differently.

### 2.5 `RollResult` keeps `die`, `modifier_sum`, `total`, AND `success`
Not just the bool. `die`/`total` are what a replay log needs to show its work (the spec's own gauntlet stage already names "log the raw work order plus the applied deltas and the RNG seed/result"); `modifier_sum` is echoed so a result reads as self-explaining without re-deriving it from the `WorkOrder` at the point it's consumed.

## 3. Verification performed

- Baseline regression, before any change: `test_gauntlet` 11/11, `test_applier` 15/15 — matches session log 6 exactly.
- `gm_rules.hpp`, `gm_gauntlet.hpp`, `gm_roll.hpp` each compiled standalone (`-fsyntax-only`, **C++20**, `-Wall -Wextra`) — clean, only the pre-existing benign `-Wmissing-field-initializers` warnings from `gm_effects.hpp`'s no-failure-branch examples.
- `test_gauntlet` and `test_applier` re-run after the `gm_rules.hpp`/`gm_gauntlet.hpp` additions — still 11/11 and 15/15, confirming no regression.
- `test_roll.cpp`: **20/20**, including the actual replay-guarantee check (two independently-constructed, content-identical `WorkOrder`s at the same `turn_number` produce an identical roll), turn-number and content each independently proven to participate in the seed, and `modifier_budget_legal()` exercised in both directions plus the exact boundary (5 legal, 6 rejected).
- `wizard_of_oz_demo.cpp` rebuilt and run end-to-end with `[ROLL]` wired for real: this run's fixed `turn_number = 1` happened to roll a **failure** (die 1, total 6 vs. DC 15) — exercised the `effects_on_failure` path for the first time in the demo (disposition −1 only, correct narration, nothing else in the world changed). Round-trip proof and both worked examples (vocabulary gate, hallucination) still pass. Exit code 0.
- Ad-hoc survey (not a shipped artifact): `resolve_turn` across 20 `turn_number` values on `bribe_the_guard`'s modifiers (magnitude 5) produced 10/20 successes — consistent with the expected ~55% rate at that modifier sum, and confirms the roll isn't stuck on one outcome.

## 4. Remaining pieces for a full Wizard-of-Oz loop

- **Validator / gauntlet control flow** — ✅ built session 5.
- **Applier** — ✅ built session 6.
- **Seeded roll** — ✅ **built this session.**
- **Logger** — the only piece left unbuilt. Everything it depends on now exists: a real seed, a real die/total, a real branch decision. Session log 6 already flagged that the logger's own contract needs the roll's seed/result — that dependency is now satisfied.

## 5. Suggested next step

**Build the logger.** Append `{raw work order, seed + roll result, applied deltas}` as one replayable event per turn, closing the last unbuilt seam named in `wizard_of_oz_demo.cpp` since session log 3. This is also the natural point to prove the *actual* end-to-end replay guarantee — not just "same inputs produce the same roll" (tested this session) but "replaying a logged run from disk reproduces the identical `World` transition, deltas included."

*Noticed, not touched this session:* `gm-adjudicator.md` §5 still lists "modifier → roll scheme" as an open question and doesn't yet credit `gm_roll.hpp`, `kDifficultyClass`, `kModifierBudget`, or the new `validate_modifiers` gauntlet stage — the spec has drifted from the artifact again, same shape session log 2 fixed once already and session log 4 flagged again. Per that established discipline, this needs a reconciliation pass before it drifts further. Flagged for the start of the next session rather than folded in here.
