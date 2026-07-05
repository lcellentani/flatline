# Session Log 9 — GM Adjudicator: Narrate Removal, hp Ceiling & Vital State

| Field | Value |
|---|---|
| **Date** | 2026-07-03 |
| **Scope** | Close two of `gm-adjudicator.md` §5's open questions end to end (brainstorm → lock → implement → verify): `narrate` vs. turn-level narration, and hp ceiling and alive/dead state. Undertaken as part of a push to get the spec to Ready for a Plan. |
| **Follows** | `session-log-8-logger.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_effects.hpp`, `gm_state.hpp`, `gm_rules.hpp`, `gm_applier.hpp`, `gm_gauntlet.hpp`, `gm_json.hpp`, `gm_log.hpp`, `room_guardroom.json`, `wizard_of_oz_demo.cpp`, `test_applier.cpp`, `test_gauntlet.cpp`, `test_log.cpp` |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

No new headers this session — every change is a modification to an already-built artifact.

Modified artifacts:
- `gm_effects.hpp` — `Narrate` struct and its `Effect` variant arm removed. 6-primitive vocabulary.
- `gm_applier.hpp` — the now-dead `apply_one(World&, const Narrate&)` overload removed; `adjust_hp` gained an upper clamp (`max_hp`) alongside the existing floor; `apply()` now derives each affected entity's `VitalState` after applying a batch; a new `applier::VitalTransition` struct and `applier::vital_transitions(world_before, effects)` function were added for the log to consume.
- `gm_gauntlet.hpp` — the two now-dead `Narrate` validation stages removed; three new preconditions added: `modify_disposition` requires `Alive`, `damage`/`modify_resource(hp)` reject a `Dead` target/actor, `transfer_item` deliberately left untouched.
- `gm_rules.hpp` — new: `vital_state()`, `hp_of()`, `kDeathOvershoot`, `derive_vital_state()`, `VitalDelta`/`vital_deltas()` (the shared batch-summing logic reused by both the applier and the log), `hp_effect_legal()`, `disposition_alive()`, `session_over()`.
- `gm_state.hpp` — new `enum class VitalState { Alive, Unconscious, Dead }`; `Entity` and `Player` both gained `max_hp` and `vital`.
- `gm_json.hpp` — `narrate` removed from the JSON shapes comment, dispatch table, and its `to_json`/`from_json` deleted; `vital_state_label`/`vital_state_from` added; `Entity`/`Player` serializers extended for `max_hp`/`vital`, tolerant of omission.
- `gm_log.hpp` — `Applied` gained a `vital_transitions` field; `VitalTransition`'s serializer reopened into `gm::applier` for ADL, same pattern already used for `RollResult` (session log 8).
- `room_guardroom.json` — `max_hp: 12` (`guard_01`) and `max_hp: 20` (`player`) added, both matching the entities' existing seeded `hp` (both already at full health).
- `wizard_of_oz_demo.cpp` — the `[LOG]` call site now passes `applier::vital_transitions(world, *applied)` into the `Applied` outcome.
- `test_applier.cpp` — 16 new checks (31 total): hp ceiling clamp, `Alive → Unconscious`, `Alive → Dead` direct, `Unconscious → Dead`, `Unconscious → Alive`, `Dead` terminality, player reaching `Dead`, `session_over()`, and `vital_transitions()`'s overkill magnitude on both a combat and a non-combat turn.
- `test_gauntlet.cpp` — 6 new checks (17 total): `modify_disposition` rejected on both `Unconscious` and `Dead`, `damage` rejected on `Dead` but still legal on `Unconscious` (contrast case), `modify_resource(hp)` rejected on `Dead`, `transfer_item` still legal against `Dead` (looting, contrast case).
- `test_log.cpp` — 6 new checks (33 total): `vital_transitions` round-trips through `Applied`'s JSON (the exact ADL-placement failure mode session log 8 already caught once for `RollResult`), and confirms an empty `vital_transitions` list serializes as `[]` rather than being omitted.
- `gm-adjudicator.md` — two open questions closed (`[x]`); two new Key Decisions bullets added (hp floor's existing bullet left as-is, a new hp-ceiling/vital-state bullet added alongside it).

## 2. Decisions locked this session

### 2.1 `narrate` removed from the effect vocabulary
Brainstormed against a capability question rather than an example hunt (deliberately — "find a case that justifies keeping it" invites motivated reasoning; "what can inline `narrate` do that a single turn-level narration field structurally cannot" doesn't). Every constructed case collapsed into `narration_on_success`/`narration_on_failure`; the one surviving candidate (per-effect attribution for a future memory/log consumer) was speculative, not a committed roadmap item. Confirmed against the actual code before deciding: `apply_one(World&, const Narrate&)` was a no-op, and both gauntlet stages (`ids_exist`, `preconditions`) trivially passed it — fully inert, zero functional payload.

*Alt rejected:* keep it dormant, same treatment as `set_flag`. The precedent doesn't transfer — `set_flag`'s dormancy is anchored to memory being a named, committed future feature in the context doc; `narrate`'s only surviving justification was generated in this session's own brainstorm, not previously committed anywhere. Locked: remove, re-add later only if a real committed consumer needs it — cheap, since `Effect` is a `std::variant` and adding an arm back is additive, not a redesign.

Zero downstream impact: no test, no demo call, no JSON fixture ever referenced it.

### 2.2 hp ceiling — Option A2
Three options were named and weighed: no cap (justified by §3's Non-Goals already excluding combat balance), a stored `max_hp` field symmetric with the already-locked floor (A2), and a per-effect magnitude cap on heals reusing the `kDamageCap`-table pattern in reverse (A3). Locked **A2**.

Sharper than initially assumed: `modify_resource(hp, +N)` had **no magnitude bound anywhere in the gauntlet** before this fix — a GM could legally grant unbounded hp with zero item involved, as pure narrative flavor on any success branch. This wasn't a future-room concern; it was reachable in v0 as already scoped, which is why it got fixed now rather than deferred alongside genuinely-unreachable items.

Numbers picked during implementation, per instruction: `guard_01.max_hp = 12`, `player.max_hp = 20` — both entities are already seeded at these exact values as their current hp, so the ceiling equals "full health," nothing invented.

### 2.3 Vital state: a flat enum, not a variant
The open question bundled hp ceiling with "0 hp carries no semantic weight" — no dead flag, nothing excludes a 0-hp entity from further interaction. Reframed mid-discussion as a genuine third axis of entity state (alongside `disposition` and `hp`, not a special case of either), prompted by a concrete scenario: a multi-turn "knock the guard out, then loot him, without killing him" sequence, checked against the actual room content (dagger cap 4, oil_flask cap 2, guard hp 12) and confirmed reachable with zero new content — this closed off treating it as deferrable the same way the trigger-boundary and failure-policy questions were.

Three representations were brainstormed: (1) an enum sibling to `disposition`, (2) reusing the `set_object_state`-style property-bag/instance-is-schema pattern, (3) a closed `std::variant` state machine with an explicit transition table, analogized to an animation state graph.

A framing was floated and then corrected during discussion: that internal type safety for illegal transitions (e.g. `Dead → Alive`) could be deferred to Experiment 2, on the reasoning that grammar-constrained decoding would guard the model later. Rejected — grammar-constrained decoding fixes the *wire format* (a spell-checker: catches malformed JSON), not *domain invariants* (a fact-checker: whether a claimed transition is legal). The gauntlet already enforces domain invariants today, for the human GM, independent of any model; deferring this specific check would reopen the exact gap being closed, just for Experiment 1 instead of Experiment 2. This ruled out building on top of the loose property-bag (option 2) without also bolting a hand-rolled legality check onto it — at which point it isn't actually looser than an enum, just less type-safe about it.

Between the enum and the variant, a second correction landed the final call: a bare three-value `std::variant` with no per-arm payload is not more compile-time-safe than a plain `enum class` — both still need the identical hand-written transition-legality check (a gauntlet-style precondition, not something the type system enforces for free). The two only diverge once a state needs to carry state-specific *data* — and the parked overkill-narration idea is exactly that kind of future consumer (a cause-of-death payload that only makes sense attached to `Dead`). Locked: **the cheaper option now** — `enum class VitalState { Alive, Unconscious, Dead }` — revisit to a variant only once that consumer, or one like it, actually ships. Known, named limit of the enum choice: cheap to add more *mutually exclusive* states later, cannot represent states *co-occurring* (e.g. poisoned and unconscious at once) without a structural change — flagged now so it reads as a stated boundary, not a surprise later.

### 2.4 Transition table and thresholds
Locked directly in discussion, not brainstormed against alternatives (thresholds were proposed and confirmed in one pass):
- `Alive → Unconscious`: batch overshoot in `[-4, 0]`
- `Alive → Dead`: batch overshoot `≤ -5`, direct — a large enough blow skips `Unconscious` entirely
- `Unconscious → Dead`: same `-5` threshold finishes off an already-downed target
- `Unconscious → Alive`: any heal that clears 0, no separate threshold
- `Dead → `: terminal for v0, explicitly reserved (not built) for a future item/magic recovery feature

`kDeathOvershoot = -5` is deliberately **batch-level, not per-effect** — the per-effect hp floor/ceiling erases the overshoot signal before a second effect in the same turn could see it, so `gm_rules.hpp::vital_deltas` sums a turn's raw (pre-floor, pre-ceiling) hp deltas per target first. Checked against the room's actual weapon caps: no single item (dagger 4, oil_flask 2) can cross `-5` alone, only combining damage effects in one work order or landing a repeated finishing blow can — deliberately hard to reach by accident, easy to reach on purpose, directly answering the "knock him out without killing him" scenario that motivated closing this question now.

### 2.5 Preconditions
- `modify_disposition` now requires the target to be `Alive` — rejects both `Unconscious` and `Dead`, not `Dead` alone (an entity that isn't fully conscious has no attitude to modify).
- Any hp-affecting effect (`damage` **or** `modify_resource(hp)`) targeting a `Dead` entity is rejected — extended from an initial `damage`-only framing once `modify_resource(hp)` was recognized as an equally valid "heal" path that could otherwise resurrect a corpse through the back door.
- `transfer_item` gets **no new gate** — locked explicitly as unchanged, not an oversight: looting or inspecting an `Unconscious` or `Dead` entity was already legal (its only precondition was ever `holds()`) and stays that way, matching the "a corpse should be lootable" requirement directly.

### 2.6 Player gets the same state set; session-over is a named, untested rule
`Player` gained `max_hp`/`vital` identically to `Entity`. Reaching `Dead` ends the session — but `wizard_of_oz_demo.cpp` runs exactly one hardcoded turn, not a real multi-turn loop (already separately named as pending, larger future work), so there is nothing to wire this into yet. `gm_rules.hpp::session_over(world)` was written and unit-tested as the rule a future loop will call, rather than either skipped or wired into a loop that doesn't exist.

### 2.7 Overkill magnitude: store at apply time, don't reconstruct
The spec previously left this "reconstructable on demand" from the log's existing `world_before`/`applied_effects` pair, to avoid building dedicated machinery just for narration. That calculus changed once the vital-state derivation above already needed to compute the exact same batch-level raw total to decide `Dead` vs. `Unconscious` in the first place — capturing it costs nothing additional at the point it's already computed. Locked: `applier::vital_transitions()` returns the per-entity `overkill` value directly; `gm_log.hpp`'s `Applied.vital_transitions` stores it, populated by the caller.

*Implementation note, not separately brainstormed:* `apply()`'s existing `World`-in/`World`-out signature was kept untouched rather than enriched to return transition data directly — that would have been a breaking change across every existing call site (`test_applier.cpp`, `test_log.cpp`'s `replay_matches`, the demo). `vital_transitions()` is a separate, additional pure function taking the same `(world_before, effects)` inputs, reusing `gm_rules.hpp::vital_deltas` so the two can't drift apart, called only by the one caller that needs it (the log-entry construction site).

## 3. Verification performed

- Set up a fresh build sandbox (headers are read-only project copies): fetched `nlohmann/json` v3.11.3 single-header (matching the version pinned in `gm_json.hpp`), copied all project sources and fixtures in.
- Compiled and ran all four existing suites **before** any change, as a baseline: `test_gauntlet` 11/11, `test_applier` 15/15, `test_roll` 20/20, `test_log` 27/27, demo exit 0 — confirmed session log 8's state reproduced exactly before touching anything.
- After the `narrate` removal alone: recompiled and reran all four suites — identical counts, zero regressions, confirming the removal was genuinely inert to remove.
- After the full hp-ceiling/vital-state implementation: recompiled all four suites (`-std=c++20 -Wall -Wextra`) — clean, no new warnings beyond the same pre-existing benign `-Wmissing-field-initializers` noise from `gm_effects.hpp`'s no-failure-branch examples and the two `Applied{...}` construction sites not yet listing the new trailing field (harmless — aggregate init value-initializes it to an empty vector).
- Ran all four: **`test_applier` 31/31, `test_gauntlet` 17/17, `test_roll` 20/20 (unchanged, untouched this session), `test_log` 33/33.**
- Compiled and ran `wizard_of_oz_demo.cpp` end to end: state dump now shows `"max_hp"` and `"vital": "alive"` on both the guard and the player; the fixed turn (bribe, failure branch) behaves identically to session log 8's run; exit code 0.
- Inspected the actual `woz_log.jsonl` output directly (not just the round-trip test): confirmed `"vital_transitions":[]` is present and correctly empty on this session's only real turn, which involves no hp-affecting effect.

## 4. Open questions status (`gm-adjudicator.md` §5)

- [x] **`narrate` vs. turn-level narration** — resolved this session.
- [x] **hp ceiling and alive/dead state** — resolved this session.
- [ ] **Same-batch effect conflicts** — untouched. Noted in passing during this session's threshold discussion (§2.4 above): the batch-summed overshoot computation is the first place this spec lets same-batch effects interact cumulatively rather than being validated in isolation, which sits directly adjacent to this still-open question. Worth remembering when it's taken up.
- [ ] **Failure policy (Experiment 2)**, **Trigger boundary (deferred)**, **Unvalidated branch hides Experiment 2 failures** — agreed in an earlier part of this session's conversation to relocate out of this spec entirely, into a future Experiment-2/movement spec, on the grounds that all three require infrastructure (a model harness, movement, a benchmark harness) that doesn't exist yet to even construct the failing case against. **Not yet applied to the spec text** — still sitting as `[ ]` in `gm-adjudicator.md` exactly as before. This is a real gap between what was decided and what's written down; flagging clearly rather than letting it pass as done.

## 5. Suggested next step

Two candidates, both already surfaced in conversation and neither blocking the other:

1. **Same-batch effect conflicts.** The one substantive open question with no decision yet at all. §2.4 above already shows this spec quietly clearing part of the ground for it (the vital-state derivation is proof that same-batch cumulative interaction is buildable when it needs to be), so this may be a smaller lift than it looked before this session.
2. **Formally relocate the three deferred items.** Purely administrative — no brainstorm needed, the decision is already made — but until it's actually written into `gm-adjudicator.md`, those three checkboxes will keep reading as unresolved blockers to Ready rather than the "out of scope for this doc" status they've actually been given.

Doing (2) first is cheap and shrinks the visible open-question count to exactly one real item before starting (1).

*Noticed, not touched this session:* `gm_gauntlet.hpp`'s top-of-file comment still names "Unvalidated branch hides Experiment 2 failures" as a known, accepted gap — correctly left as-is, since that item wasn't resolved this session, only discussed for relocation. Don't "fix" that comment's wording until the spec text itself actually moves the question out.
