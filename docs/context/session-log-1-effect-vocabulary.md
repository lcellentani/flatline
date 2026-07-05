# Session Log 1 — GM Adjudicator: Effect Vocabulary & One-Call Decision

| Field | Value |
|---|---|
| **Date** | 2026-07-01 |
| **Scope** | Closing spec open question "vocabulary membership" and open question #3 ("one model call or two") |
| **Reads alongside** | `llm-game-master.md` (context), `gm-adjudicator.md` (spec, v0), `gm_effects.hpp` (artifact produced this session) |
| **Purpose** | Let a human or a model pick this project back up without re-deriving anything below. |

---

## 1. What this session produced

**Artifact:** `gm_effects.hpp` — a C++ header realizing the spec's §4 "Effect vocabulary (representative v0)" as a `std::variant`, plus the `WorkOrder` struct that carries a turn's ruling, roll modifiers, and effects. Contains three worked examples (bribe, burn-door, hallucination) as compile-time sanity checks.

This closes two of the spec's five open questions (§5):
- **Vocabulary membership** — locked as a starter set (see §2 below).
- **One model call or two** — locked as one call (see §3 below).

Three open questions remain untouched (roll scheme numbers, Experiment 2 failure policy, movement trigger boundary) — see §5.

---

## 2. Decisions locked this session

### 2.1 The effect vocabulary (7 primitives)
`Damage`, `ModifyDisposition`, `TransferItem`, `ModifyResource`, `SetObjectState`, `SetFlag`, `Narrate` — matches the spec's representative list exactly, now typed as a `std::variant<...>`.

### 2.2 `VOID_ENTITY` — formalizes "consumed"
The spec's burn-door example used an informal `<consumed>` target for `transfer_item`. Formalized as a reserved sink id, `EntityId{"__void__"}`. `transfer_item(item, from, VOID_ENTITY)` *is* destruction — no new `destroy_item` primitive needed, consistent with the spec's locked "closed vocabulary, map novel actions onto existing primitives" decision.

### 2.3 Id design — tagged types, not raw strings
`Id<Tag>` (`EntityId`, `ObjectId`, `ItemId`) wraps a `std::string` but uses a phantom template tag so the three id kinds are distinct types to the compiler, even though all three are strings underneath and get resolved by string match against live state in the gauntlet's existence check. Catches "passed an `ObjectId` where an `EntityId` was expected" at compile time, at zero runtime cost.

### 2.4 `PropertyValue` — closed variant for object properties
`set_object_state` touches heterogeneous fields (`locked: bool`, `material: string`, ...). One `std::variant<bool, int64_t, std::string>` keeps the primitive singular instead of splintering into per-type setters.

### 2.5 Bounds/clamping live in the validator, not the type
`Damage.amount`, `ModifyDisposition.delta` etc. are plain numbers in the struct. Clamping (weapon max, disposition enum range) is explicitly the gauntlet's "magnitude in bounds" stage's job — the type doesn't self-enforce it. (This is *why* gap §4.3 below matters — the clamping logic has nowhere to read bounds from yet.)

### 2.6 `ruling` is a scope decision, not the roll's outcome
Clarified mid-session: `ruling` (allow/partial/deny) is fixed by the GM at interpretation time — "of what the player asked for, how much is even on the table" — independent of any dice roll. Example: "bribe the guard to open the door AND abandon his post" → `ruling = partial` because "abandon post" is refused at the scope level, permanently, regardless of roll outcome. The roll only decides success/failure on whatever `ruling` left in play. This distinction was not explicit in the original spec language and is worth folding back into `gm-adjudicator.md` if useful.

### 2.7 One call, not two (closes spec §5 open question #3)
**Decision:** the GM authors both outcome branches in a single call. `WorkOrder` now carries:
```
modifiers                              // feeds the engine's roll; empty => no roll, success is automatic
effects_on_success / narration_on_success
effects_on_failure / narration_on_failure   // stays empty when modifiers is empty — no roll, no failure branch
```
**Why (over two calls — modifiers first, effects authored after seeing the roll):**
- Halves inference calls per turn — directly protects the platform's latency-hiding bet (turn-based play absorbs LLM latency by minimizing round-trips; context doc §5–6).
- E4B sits *at* the reliable-structured-output floor (context doc §5) — a second inference dependency per turn multiplies exposure to that exact failure mode.
- Wasted generation (authoring an unused branch) is a cheap cost to eat; a broken/re-prompted turn is not.
- Two-call's advantages (smaller per-call schema, zero wasted branch, natural handling of roll *margin*) are real but are things you can layer on later without touching the vocabulary — not a one-way door.

**Side effect of this decision (flagged, not resolved):** the `Narrate` effect primitive is now partially redundant with `narration_on_success`/`narration_on_failure`. Left in `gm_effects.hpp` with a comment rather than removed — open whether individual effects (vs. the whole turn) ever need their own inline flavor text. Candidate for removal once real usage from Experiment 1 shows whether that distinction earns its keep.

---

## 3. Gaps identified — effect vocabulary (still open, untouched by the one-call decision)

These are orthogonal to §2.7 — they're about the effect *types* and the data the validator needs, not about `WorkOrder`'s call shape.

1. **Precondition rules per effect** — the gauntlet's "preconditions met" stage (e.g. "guard must currently hold the key") has no table yet mapping effect type → required state condition. Currently only exists as prose in the spec's worked examples.
2. **Object property schemas** — nothing defines, per object type, which properties are legal (`door_north` has `locked: bool`, `material: string` — but that's not encoded anywhere the validator can check `SetObjectState` against).
3. **Bounds sources** — no lookup table yet for weapon/source → max damage, or for the disposition enum's actual numeric range. The type clamps to *some* range; that range isn't defined.
4. **`ModifyResource::Resource` completeness** — currently only `Gold` and `Hp`. Not yet deliberately confirmed whether the v0 single room needs more (e.g. a light-source resource).
5. **Serialization bridge (JSON ↔ `WorkOrder`)** — Experiment 1's human GM will fill out work orders as JSON (or a form), not C++ struct literals. No parser/serializer exists yet. **This is the most likely next blocker** if the near-term goal is to actually run the Wizard-of-Oz session.

---

## 4. Spec open questions untouched this session

From `gm-adjudicator.md` §5 — still open, not discussed this session:

- **Modifier → roll scheme** — the numeric scale modifiers use and how the engine consumes them (d20 + mods vs. probability target). Locked in principle ("the d20 stays a real d20"); the numbers are open. Note: this is now *more* concrete after the one-call decision, since `effects_on_success`/`effects_on_failure` presuppose a binary pass/fail roll — worth revisiting together.
- **Failure policy (Experiment 2)** — on malformed/illegal model output, re-prompt, fall back to `deny`, or clamp? Matters once the real E4B harness exists.
- **Trigger boundary (deferred)** — once movement exists, what condition flips a free deterministic action into an adjudicated one. Explicitly moot for the single-room v0.

---

## 5. Suggested next step

Two natural candidates, not yet chosen:
- **Gap §3.5 (serialization bridge)** — likely the actual blocker to running Experiment 1 at all.
- **§4 modifier → roll scheme** — now sharper given the one-call decision locks in a binary success/failure branch structure.

No decision made on which to tackle first — flag this at the start of the next session.
