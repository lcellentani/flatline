# Session Log 3 — GM Adjudicator: Serialization Bridge

| Field | Value |
|---|---|
| **Date** | 2026-07-01 |
| **Scope** | Build the JSON ↔ `WorkOrder`/state bridge to unblock the Experiment-1 (Wizard-of-Oz) loop. |
| **Follows** | `session-log-2-review-reconciliation.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_effects.hpp`, `gm_state.hpp`, `gm_json.hpp` |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

New artifacts:
- `gm_state.hpp` — the engine-owned **state model** in code (World/Room/ObjectState/Entity/Player), matching spec §4. Forced into existence by the bridge: you cannot serialize a type that doesn't exist.
- `gm_json.hpp` — the **serialization bridge**: pure JSON ↔ struct translation for both the work order and the state, using nlohmann/json.
- `wizard_of_oz_demo.cpp` — a runnable loop skeleton that exercises the bridge and marks where the unbuilt pieces plug in.
- `room_guardroom.json`, `workorder_bribe.json` — an authored scenario and a sample work order (the bribe from the spec).

## 2. Decisions locked this session

### 2.1 JSON is the canonical bridge; a REPL is a later, optional layer
The human GM authors the *same JSON the model will emit* (dual-reader, spec §2), so Experiment 1's parser is byte-for-byte Experiment 2's parser and Experiment 1 honestly tests the model's target format. A friendlier REPL/form can sit on top later without touching the bridge — not a one-way door. If turn-by-turn JSON proves too slow in play, that is the trigger to add it.

### 2.2 nlohmann/json (single header, pinned v3.11.3)
Standard, header-only, ergonomic. The same library will consume the grammar-constrained model output in Experiment 2, so no second parser is introduced later.

### 2.3 The bridge is pure translation — no validation
Not a rule lives in the bridge. Ids are echoed as strings; nothing is checked for existence, preconditions, or bounds. That is all the validator gauntlet's job, downstream. This keeps the seam (spec "How It Fits") intact: swap the producer (human ↔ model), the bridge and everything after it are unchanged.

### 2.4 Closed vocabulary is enforced at the parse boundary
An effect whose `"type"` is not one of the seven primitives throws at `from_json` — before the gauntlet. So "reject an unknown *verb*" is structural and free; "map novel *intent* onto existing primitives" remains the GM's job upstream of the JSON. (Verified: a `teleport` effect is rejected.)

### 2.5 State representation choices
- **disposition** is stored as an ordinal `int` (so `modify_disposition`'s delta arithmetic is well-defined; `hostile −2 … allied +2`), but serializes as a readable **label** and parses from either label or int. State dumps are shown to the GM, so legibility matters.
- **ids** serialize as plain JSON strings (the GM only ever sees ids as strings).

### 2.6 Implementation note (for whoever extends the bridge)
`PropertyValue` is `std::variant<bool,int64_t,std::string>`; its associated namespace is `std`, so plain `gm::to_json/from_json` are invisible to nlohmann's ADL. It needs an `adl_serializer<gm::PropertyValue>` specialization. (`Effect`, also a variant, is fine — `gm` is associated via its element types.) If you add a primitive that carries another `std`-namespaced type, expect the same and specialize.

## 3. Verification performed

- Compiles and runs on **C++20** (`-Wall -Wextra`); requires C++20 (inherited from `gm_effects.hpp`).
- State parse → dump round-trips; work order parse → struct → dump is **identical** to the input JSON (the property the replay log and the dual-reader both rely on).
- Unknown effect `"type"` is rejected at the bridge.
- Benign `-Wmissing-field-initializers` warnings persist from `gm_effects.hpp`'s no-failure-branch examples.

## 4. Remaining pieces for a full Wizard-of-Oz loop

The bridge is one of four. Still unbuilt (marked as seams in `wizard_of_oz_demo.cpp`):
- **Validator / gauntlet** — the critical path next. It needs the spec's still-open **"validator reference data"** (§5): per-effect precondition rules, per-object property schemas, source→max bounds tables. The bridge can't proceed to a *playable* loop without at least a first cut of these.
- **Seeded roll** — consume `modifiers`, pick a branch. Depends on the still-open **modifier → roll scheme**.
- **Applier** — apply the surviving branch's deltas atomically to the `World`.
- **Logger** — append `{raw work order, seed+result, applied deltas}` as one replayable event.

## 5. Suggested next step

**The validator gauntlet**, because it is the gate between "the bridge parses a work order" and "a turn actually resolves." Its blocker is no longer code — it's the **validator reference data** open question. So the next working session is really: enumerate the precondition rules, object property schemas, and bounds for the seven primitives against the guardroom scenario, then implement the gauntlet on top. That single open question now sits on the critical path to running Experiment 1 at all.
