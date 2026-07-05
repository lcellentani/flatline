# Session Log 4 — GM Adjudicator: Validator Reference Data

| Field | Value |
|---|---|
| **Date** | 2026-07-01 |
| **Scope** | Lock the validator's reference data (preconditions, magnitude bounds, object schema, storage location) and implement it — closing session log 3's identified blocker. |
| **Follows** | `session-log-3-serialization-bridge.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_effects.hpp`, `gm_state.hpp`, `gm_json.hpp`, `gm_rules.hpp` |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

New artifact:
- `gm_rules.hpp` — the hardcoded C++ reference data the validator gauntlet will consult: the damage-cap table, a shared `holds()` helper (resolves the `Entity.holds` / `Player.inventory` asymmetry), `disposition_eligible()`, `gold_spend_legal()`, `object_property_legal()`, plus documented (not coded) conventions for cases with nothing to reject.

Modified artifacts:
- `gm_effects.hpp` — added the `UNARMED_ITEM` sentinel (same pattern as `VOID_ENTITY`); added a required `via: ItemId` field to `Damage`, replacing the old "resolves to source's max" ambiguity.
- `gm_json.hpp` — `Damage`'s `to_json`/`from_json` updated to carry `via`; top JSON-shape comment updated to match.
- `workorder_bribe.json` — corrected `-20` → `-15` gold (and the interpretation text), matching both the player's actual 15-gold balance in `room_guardroom.json` and the modifier label that already said "generous offer on a 15-gold purse."
- `gm-adjudicator.md` — same 20→15 correction in two more spots (Key Decisions, Worked Examples); `damage`/`modify_disposition` vocabulary lines updated; gauntlet description expanded to name the sentinel carve-out and snapshot-vs-folding semantics; **"Validator reference data" open question resolved and closed**; two new open questions added (hp floor with no ceiling; same-batch effect conflicts); Contract Schemas section now credits `gm_rules.hpp` alongside `gm_effects.hpp`.

## 2. Decisions locked this session

### 2.1 Damage / weapon resolution
- `Damage.via: ItemId` is **required**, never omitted — no silent fallback to an unscoped "unarmed" default.
- `UNARMED_ITEM` sentinel, same pattern as `VOID_ENTITY` — "no weapon" is a stated value, not an absence.
- Closed damage-cap table, one entry per item that can exist in room state: `dagger=4`, `oil_flask=2`, `key_north=1`, `UNARMED_ITEM=1`. Every physical item gets a row whether or not combat is its "intended" use (a key to the face still hurts) — same closed-vocabulary stance already locked for effects, applied one level down to weapons.
- A lookup miss on the damage table is a **table gap** (someone forgot to cover a real item), distinct from "item doesn't exist" (the hallucination check, one gauntlet stage earlier). The two must not be reported under the same label.
- `modify_resource(hp)` is reserved for untargeted causes (traps, potions); any entity-vs-entity hp change must route through `damage`. Documented convention, not gauntlet-enforced — the validator cannot detect a GM routing around the cap this way.

### 2.2 Shared lookups
- `holds(world, id, item) -> bool` — one helper used by both `transfer_item`'s `from`-precondition and `Damage.via`'s precondition, resolving the `Entity.holds`/`Player.inventory` field-name asymmetry once.
- Effect-to-id-class eligibility: `modify_disposition` is the only v0 effect that rejects the player id (Player has no `disposition` field); everything else targeting an `EntityId` accepts either.

### 2.3 Object property schema
- **Instance-is-schema** — an object's legal properties are exactly the keys it was seeded with in the room JSON; a `set_object_state` effect is legal only if the property already exists and the new value's type matches the existing one's. Revisit once a second room needs per-object-type schemas.

### 2.4 Remaining preconditions
- `transfer_item(from == to)` — allowed as a no-op.
- Self-`damage` (`source == target`) — allowed.
- `modify_resource(gold)` going negative — **rejected** as a precondition (not clamped), symmetric to `transfer_item`'s holder check. This is what forced the `workorder_bribe.json` correction.
- Damage to an already-0-hp target — explicitly **deferred**, not resolved. No alive/dead state exists anywhere in `gm_state.hpp` yet.
- Existence-check carve-out — `VOID_ENTITY`/`UNARMED_ITEM` are sentinels, never real ids; the gauntlet's "referenced ids exist" stage must special-case both explicitly or it breaks the canonical `burn_the_door()` example by construction.
- Batch validation runs against a **single pre-turn snapshot**, not folding state effect-by-effect — consistent with "apply atomically" already locked in the spec. Same-batch conflicts (two effects fighting over one item) are a named, deferred gap.

### 2.5 Storage location
- Hardcoded C++ tables (`gm_rules.hpp`), matching the "representative, not final" spirit already locked for the effect vocabulary. Promote to data-driven storage only once a second room makes hand-editing this file hurt.

## 3. Verification performed

- Compiled `gm_effects.hpp` + `gm_state.hpp` + `gm_rules.hpp` together under **C++20** (`-Wall -Wextra`); exercised `holds()`, `damage_cap()`, `disposition_eligible()`, and `gold_spend_legal()` against a hand-built world — all assertions pass.
- Compiled `gm_json.hpp` against a real nlohmann/json header (pulled via the pip mirror for this check only; project pins v3.11.3, verification used the current v3.12.0 — API-compatible for what's exercised here, not re-pinned).
- Round-tripped a `Damage` effect carrying `via` through JSON; confirmed a `Damage` missing `via` throws at `from_json` — the required-field contract is real, not just a comment.
- Ran `wizard_of_oz_demo.cpp` end-to-end against the corrected `workorder_bribe.json` and `room_guardroom.json`: state prints correctly, the work order parses with the corrected 15-gold interpretation, round-trip is identical, and the closed-vocabulary gate still rejects an invented `teleport` effect.
- Benign `-Wmissing-field-initializers` warnings persist from `gm_effects.hpp`'s no-failure-branch examples (unchanged from session log 3 — not introduced by this session).

## 4. Remaining pieces for a full Wizard-of-Oz loop

Updated from session log 3's list — one item's blocker just cleared:

- **Validator / gauntlet control flow** — now the sole remaining blocker on the critical path. The reference *data* it needs exists (`gm_rules.hpp`); the function that actually sequences schema-valid → ids-exist (with the sentinel carve-out) → preconditions → magnitude → apply → log is still unbuilt.
- **Seeded roll** — unchanged; still depends on the open **modifier → roll scheme** question. Not a blocker for building the gauntlet itself, since the no-roll path (`burn_the_door`) and instant-reject paths (`hallucinated_item`) don't need a roll scheme to test.
- **Applier** — unchanged, unbuilt.
- **Logger** — unchanged, unbuilt.

## 5. Suggested next step

**Build the gauntlet control flow.** All the data it needs to call into now exists in `gm_rules.hpp`; what's missing is the function that walks a `WorkOrder`'s selected effect branch through the ordered stages and reports which one failed (and why — table-gap vs. hallucination vs. precondition, kept as distinct outcomes per §2.1/§2.4 above). It can be built and tested against all three existing worked examples (`bribe_the_guard`, `burn_the_door`, `hallucinated_item`) without the roll scheme, since none of them need the roll's actual numeric behavior to exercise every gauntlet stage — bribe just needs its `modifiers` list to *not* be empty. The roll scheme becomes load-bearing only once the gauntlet needs to pick a branch, not before.

*Noticed, not touched this session:* the spec's closing "near-term note" still describes the JSON serialization bridge as future tooling — it shipped in session log 3. Not part of today's locked decisions; flagged for whoever next edits that section.
