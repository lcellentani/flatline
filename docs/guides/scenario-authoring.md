# Scenario Authoring Guide — grouping WorkOrders into narrative threads

| Field | Value |
|-------|-------|
| **Purpose** | Practical reference for organizing individual WorkOrders (`docs/guides/gm-workorder-rulebook.md`'s subject) into named narrative scenarios, and for enriching room state to support them |
| **Companion** | `docs/guides/gm-workorder-rulebook.md` covers how to fill out one WorkOrder correctly; this guide covers designing a *set* of WorkOrders that read as a coherent scenario |
| **Contract** | `docs/specs/gm-adjudicator.md` is authoritative for anything mechanical below; this doc never overrides it |
| **Scope** | v0 guardroom (`res/room_guardroom.json`) and its fixtures under `res/workorder_*.json` |

---

## 1. What a "scenario" is

A **scenario** is a named narrative thread within a room: a coherent player strategy or goal, plus the cluster of WorkOrders that realize it — the attempt, its complications, its escalations. It is not the same thing as a WorkOrder (one turn) or a room (the whole state space) — it's the layer in between.

Guardroom scenarios identified so far (2026-07-31, extended 2026-08-01):
- **The Bribe** — social path
  - `workorder_bribe.json` — baseline: offer the full 15-gold purse for the key.
  - `workorder_bribe-second-attempt.json` — retry after `workorder_bribe.json`'s failure branch; modifiers are tuned harder (net negative) to reflect the guard's dropped disposition. Depends on that prior failure — not a fresh-state opener.
  - `workorder_bribe-goods.json` — alternate offer: half the purse (5 gold) plus the oil flask, instead of all 15 gold. A fresh-state *alternative* to the baseline, not a sequel — don't chain it after `workorder_bribe.json` in the same session, both assume the guard still holds `key_north` and the player still has their starting inventory.
- **The Heist** — stealth path, now a three-step chain instead of one fixture:
  - `workorder_heist-distract.json` — pay 10 gold for the guard to look away. Doesn't move `key_north` — it only buys the "distracted" narrative fact that `workorder_open-door.json`'s existing `guard distracted +2` modifier always assumed but never established. Per §6/§7, success here is a GM-authored fact carried forward by convention, not an engine flag — nothing downstream checks it automatically.
  - `workorder_heist-steal-key.json` — lift `key_north` off the guard while distracted. Narratively follows a successful `heist-distract`, but is not mechanically gated on it (its own `guard distracted +2` modifier is asserted the same way `open-door.json`'s always was).
  - `workorder_open-door.json` — unchanged; unlocks the door and slips out. Was already written assuming the player holds `key_north` and the guard is distracted — `heist-distract` + `heist-steal-key` are what now actually earn that precondition instead of leaving it implicit.
- **The Fire Gambit** — sabotage-turned-violent-escalation path (`workorder_burn-door.json`, `workorder_lethal-smoke.json`, `workorder_push-guard.json`)

Kept **outside** any scenario group on purpose: `workorder_hallucinated.json` is an adversarial schema-stress case (a `silver_key` that doesn't exist), not a narrative attempt — don't force adversarial fixtures into a scenario group just because they share a room.

Scenario groups are allowed to stay independent even when they touch thematically similar material (e.g. two different smoke sources) — don't merge them into one causal chain unless that's an explicit, discussed decision. (2026-07-31: a proposal to retcon `workorder_lethal-smoke.json`'s "brazier" into a new floor-trigger trap was considered and rejected — they stay two separate hazard concepts.)

## 2. Naming convention

All WorkOrder fixtures live in `res/` and share a mandatory `workorder_` prefix — CMakeLists.txt's `FLATLINE_WORKORDER_FIXTURES` filter (`REGEX INCLUDE "/workorder_"`) is what copies them into `woz_gui`'s Saved-folder picker at build time; a file without that prefix silently never reaches the GUI. Beyond that fixed prefix:

`workorder_<action-slug>.json` — a short, hyphenated, verb-first description of the move itself (`bribe`, `open-door`, `burn-door`, `push-guard`). A named variant of an existing scenario extends the slug: `workorder_<scenario-slug>-<variant-slug>.json` (e.g. `workorder_bribe-goods.json`, `workorder_heist-distract.json`).

Deliberately **not** a numbered/positional scheme (e.g. `SC1-01`, `SC1-02`): a sequence number implies a fixed play order, which fights the mix-and-match goal — a human GM should be able to reach for whichever fixture fits the moment, independent of authoring history. Scenario *membership*, and any real play-order dependency between fixtures, is tracked in §1's roster prose, not baked immutably into the filename — that's also what let `workorder_heist-distract.json` land in the Heist group even though the action itself is a bribe (see §1): the filename reflects which scenario the fixture currently serves, not a rigid taxonomy of the verb.

Pre-2026-08-01 fixtures keep their original names (no rename churn); this convention applies going forward.

## 3. `player_input` voice

`player_input` (the raw-words field, distinct from `interpretation`'s GM paraphrase — see `docs/specs/player-input-logging.md`) should read like the output of a game orchestrator translating real keyboard/mouse input, not free chat prose — this is a keyboard/mouse roguelike, not a MUD. The right voice depends on interaction channel (this previews DD-3's still-open "input channel union" question — `docs/context/investigation-plan-experiment-2.md` DD-3 — informally, for v0 fixture-authoring purposes only):

- **Mechanical/spatial actions** (`interact`, `use_item_on`, `attack`, a triggered movement) — a short, third-person, orchestrator-style phrase: `"player moves to door_north and interacts"`, `"player uses oil_flask on door_north"`, `"player attacks guard_01 with dagger"`.
- **Dialogue exchanges** — closer to verbatim/quoted speech: `"player says: 'Take this and let me go' (offers 15 gold)"`.

Don't write `player_input` as a full narrated sentence in the GM's voice — that's what `interpretation` and the narration fields are for.

## 4. Every narrative detail needs state behind it

If a WorkOrder's `interpretation` or narration references an object, item, or hazard, that thing must exist in `room_guardroom.json` with real seeded properties — no floating flavor text that implies state the room fixture doesn't actually have.

Named example of the gap this rule exists to catch: `workorder_lethal-smoke.json`'s narration references "acrid brazier smoke," but `room_guardroom.json` has no brazier object today — the reference is currently unbacked. Not retconned as of this writing (§1) — flagged here as a known gap, not silently fixed.

## 5. New objects are cheap; new primitives are not

Adding an object to a room fixture (e.g. a floor trigger plate) is just data — free, and doesn't require touching the engine. Adding a new entry to the **effect vocabulary** (the six primitives: `damage`, `modify_disposition`, `transfer_item`, `modify_resource`, `set_object_state`, `set_flag`) is a locked v0 boundary — `gm-adjudicator.md` §4 locked it as a *representative, not exhaustive* starter set, extended only when a player intent genuinely doesn't decompose into the existing six (§3 Non-Goals). Default to expressing new scenario ideas as new objects/state plus existing primitives; treat "we need a new primitive" as a rare, explicitly-flagged exception.

## 6. Multi-turn / persistent hazards are GM-authored, not engine-automated

v0 has no timer, no turn counter, and no mechanism that ticks an effect on its own between WorkOrders. A hazard meant to escalate over several turns (e.g. lingering smoke that gets worse the longer the player stays) must be **re-authored by the human GM every turn it persists** — magnitude chosen at the GM's narrative discretion each time, the same already-accepted gap `modify_resource(hp)` has for untargeted causes (`gm-adjudicator.md` §4, "modify_resource(hp) is for untargeted causes only").

Do not design a scenario that assumes the engine enforces a clock, an escape window, or automatic recurring damage on its own — that would cross into the explicitly deferred "semantic memory / persistent world reactions" non-goal (`gm-adjudicator.md` §3). "Escape in N turns" is a human-GM bookkeeping convention for Experiment 1, not a mechanical guarantee — say so explicitly in the scenario's notes so a future reader (or a future model) doesn't assume automation that isn't there.

## 7. Object-trigger convention (movement that becomes a WorkOrder)

A player action that flips *adjudicable* object or entity state (e.g. stepping on a trigger plate that arms a trap) is a WorkOrder, not free movement — per the still-open DD-2 leaning that the Tier-0/WorkOrder boundary is "anything touching adjudicable state" (`docs/context/investigation-plan-experiment-2.md` DD-2). Ordinary movement that touches nothing adjudicable stays free and off the log. This is a leaning, not a locked decision — DD-2 hasn't concluded — but it's the working assumption for authoring guardroom scenario content today.

## 8. Scenario checklist

- [ ] Does the scenario map to a single coherent player strategy, distinct from the other named groups?
- [ ] Does the fixture's filename follow §2 — `workorder_<scenario-slug>-<variant-slug>.json`, no positional numbering?
- [ ] Is every object/item/hazard referenced in narration actually seeded in `room_guardroom.json`?
- [ ] Does `player_input` on each fixture read like translated input (§3), not GM-voice prose?
- [ ] If the scenario spans multiple turns, is any persistence/escalation clearly a GM-authored convention, not an assumed engine mechanic (§6)?
- [ ] Does every new WorkOrder still decompose into the existing six primitives (§5) — and if not, is that flagged explicitly rather than quietly bent?
- [ ] Are adversarial/schema-stress fixtures (like `workorder_hallucinated.json`) kept out of narrative scenario groups?
- [ ] If a WorkOrder assumes a prior turn's outcome (a retry, or a chained precondition like `heist-steal-key` following `heist-distract`), is that dependency spelled out in §1's roster, since the JSON schema has nowhere to record it?

---

*Companion to `docs/guides/gm-workorder-rulebook.md`. Update both if a scenario surfaces a gap in the mechanical rulebook itself — this doc is for grouping and room-content decisions, not a substitute for it.*
