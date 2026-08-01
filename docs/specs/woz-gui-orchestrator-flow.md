# woz_gui Orchestrator Flow — SPEC

| Field | Value |
|-------|-------|
| **Status** | Draft |
| **Date** | 2026-08-01 |
| **Plan** | Not yet planned |
| **Supersedes** | N/A |

---

## 1. Problem

`woz_gui.cpp`'s GM WorkOrder panel represents a player turn as one flat `player_input` text box tied 1:1 to whatever WorkOrder the GM is about to submit (`player-input-logging`, Implemented). That model has no place for the explore/evaluate/inspect activity that precedes an adjudicable action — a player looking at the guard, checking their own inventory, or trying an unlocked exit never gets a `player_input` line and never gets logged, because nothing about it produces a WorkOrder. Two consequences: the GM hand-types every line from scratch with no structural help enforcing the orchestrator-voice convention (`docs/guides/scenario-authoring.md` §3), and the session log — `gm::log::LogEntry`, one line per adjudicated turn — only ever captures the adjudicated subset of a session, never the exploratory beats around it. `session-log-13` already named this corpus-thinness gap as what's blocking DD-1's evidence audit; it's also untested ground for DD-2's Tier-0/WorkOrder boundary leaning and DD-3's structured-input-channel leaning, both still open in `docs/context/investigation-plan-experiment-2.md`.

## 2. Goals

- The GM WorkOrder panel gains a structured action-composition control — a `kind` selector (`examine`, `move`, `interact`, `use_item_on`, `attack`, `say`) plus contextual target/item/direction fields populated from the currently loaded room's real objects/entities — that assembles a canonical orchestrator-voice line per `docs/guides/scenario-authoring.md` §3, rather than requiring the GM to hand-type it.
- Composing an `examine` action against any object or entity resolves immediately and inline: a read-only description drawn from that target's already-seeded properties (`hp`, `disposition`, `holds`, `persona`, object properties) is shown to the GM, no WorkOrder editor involved.
- Composing a `move` action toward an exit whose gating object currently permits passage (e.g. `locked: false`) resolves the same way — inline, no WorkOrder.
- Composing `interact`, `use_item_on`, `attack`, `say`, or `move` toward a currently-blocking exit surfaces the composed line as a **hint**, not a literal pre-fill, in the existing `player_input` field, and the GM proceeds to author a WorkOrder as today.
- `gm::log::LogEntry` can represent a Tier-0 beat as well as an adjudicated turn: `Outcome` gains a new `Observed` alternative, and `work_order` becomes optional so a session log line no longer requires a WorkOrder to exist.
- A single session's JSONL log — read start to end — reconstructs the whole playthrough (Observed and adjudicated lines together, in turn order), not just the adjudicated subset.
- The composed/hinted input sits above the WorkOrder editor in the panel layout, reflecting explore → `player_input` → GM causality instead of today's flat, simultaneous arrangement.

## 3. Non-Goals

- **A stored player_input example table with auto-fill-on-Load / write-back-on-Save** — considered and dropped this session. The compositor generates a line live, from the room state actually in front of the GM at that moment; there's nothing to pre-author or persist ahead of time.
- **Resolving `gm-adjudicator.md` §5's relocated "trigger boundary" question, or building a real movement system** — that question is explicitly deferred until movement exists as a first-class system. The detector this spec adds is a `woz_gui`-side heuristic assisting the human GM in Experiment 1, not an engine-level answer to that question. Evidence from using it may inform a future DD-2 dive, but this spec doesn't claim to conclude DD-2.
- **Any LLM/inference involvement** — this is Experiment-1 (human-GM) tooling; nothing here touches Experiment 2's harness or changes DD-3 from a leaning to a decision.
- **Changes to the six-primitive effect vocabulary, `gm_gauntlet.hpp`, `gm_roll.hpp`, or `gm_applier.hpp`** — the detector decides only whether adjudication is needed, never whether an action will succeed; legality stays entirely the gauntlet's job.
- **`woz_loop.cpp` / `wizard_of_oz_demo.cpp` changes** — per `player-input-logging`'s precedent, `woz_gui.cpp` is the live Experiment-1 tool. Both other front ends keep constructing only `Applied`/`GauntletRejected`/`ModifierBudgetRejected` outcomes with a real `WorkOrder`, so the `Outcome`/`LogEntry` schema changes here don't force any call-site changes there.
- **A general-purpose multi-room compositor** — the target picker enumerates whatever the currently loaded `World` actually contains; no work toward supporting rooms beyond v0's single guardroom.

## 4. Design

### Key Decisions

- **Detection principle: adjudication is needed iff resolving the action would apply one of the six effect primitives against current state — not whether it would succeed.** Even an action headed for an obvious `deny` still goes through the WorkOrder shape per `gm-adjudicator.md` (empty effect branches, refusal narration); "does this touch adjudicable state" and "will this be allowed" are different questions, and only the first is mechanically decidable ahead of time. This is what makes a deterministic (non-LLM) detector legitimate for Experiment-1 tooling.
- **Per-`kind` rule table, locked for v0:**
  - `examine` — always Tier-0 (`Observed`). This kind has no mapping onto any of the six primitives; it only ever reads existing state.
  - `move` — conditional: Tier-0 if the destination exit's gating object doesn't currently block passage, WorkOrder otherwise. The one kind whose classification depends on target state, not just the kind itself.
  - `interact`, `use_item_on`, `attack` — always route to WorkOrder. These kinds inherently propose a state change (unlock, transfer, damage), so the tool never pre-guesses legality; that stays centralized in the gauntlet.
  - `say` — always routes to WorkOrder, same as the three above. Dialogue resists clean slot-based classification (nothing distinguishes idle color from an implicit demand structurally); defaulting to "needs the GM" is the conservative choice, since a false Tier-0 classification would let a real demand slip past adjudication ungoverned — worse than an occasional unnecessary GM look at idle chatter.
- **Hint, not literal pre-fill, for the WorkOrder-bound cases.** The composed line populates `player_input`'s hint text (e.g. `ImGui::InputTextWithHint`), not its buffer. Alternative rejected: writing the composed line directly into the buffer — this would let a GM submit an unedited scripted line as if it were the real player's words, silently corrupting exactly the raw-input signal `player-input-logging` exists to protect. The hint preserves that spec's "empty ⇒ no raw input captured" invariant: an unedited field still submits empty.
- **`Outcome` gains an `Observed` alternative; `LogEntry.work_order` becomes `std::optional<WorkOrder>`.** Alternative considered: giving Tier-0 beats a synthetic placeholder `WorkOrder` so `work_order` could stay required — rejected as misleading data (a `WorkOrder` implies a GM ruling that never happened) for no real benefit over an optional field, and `gm_log.hpp` already has a tolerant-of-omission precedent (`player_input`, `conflicting_with_index`) to extend rather than invent from scratch.
- **Tier-0 resolution bypasses `process_turn` entirely** — it never touches the gauntlet, roll, or applier, so it doesn't belong inside `gm_turn.hpp`'s existing pipeline function. A separate, much smaller path reads `World` directly, builds the `Observed` outcome, and appends the resulting `LogEntry` the same way `process_turn`'s callers already do today.
- **Layout: composition/hint control precedes the WorkOrder editor**, replacing the current arrangement where `player_input` sits as a footer field beside it — makes the panel's visual order match the causal order (explore → input → GM ruling) instead of implying the two are filled together.

### How It Fits Existing Systems

- `woz_gui.cpp` — the primary surface: new compositor controls (kind selector, target/item/direction fields sourced from the loaded `World`'s objects/entities), the Tier-0 inline-resolution path, the hint-based `player_input` population (replacing the current plain `InputText`), and the panel layout reorder. Builds directly on the inline field `player-input-logging` added (`woz_gui.cpp:558-561`).
- `gm_log.hpp` — `Outcome`'s `std::variant` gains `Observed`; `LogEntry.work_order` changes from `WorkOrder` to `std::optional<WorkOrder>`; `to_json`/`from_json` for both follow the existing flattened-object, tag-on-`"status"` pattern already used for the other three outcomes.
- `gm_state.hpp` / `room_guardroom.json` — read-only consumer: the compositor's target picker and the `move` conditional check both read `World`'s already-seeded `Room.exits`, `objects`, and `entities` — no schema changes.
- `gm_turn.hpp` — untouched; `process_turn` continues to handle only adjudicated turns. The new Tier-0 path is separate code, not a new branch inside it.
- `test_log.cpp` — extended with round-trip cases for `Observed` and for a `LogEntry` with `work_order` absent, following the same present/empty/absent-on-old-lines shape already used for `player_input`.
- `woz_loop.cpp`, `wizard_of_oz_demo.cpp` — no changes; both always construct a real `WorkOrder`, so the now-optional field doesn't affect them.

## 5. Open Questions

- [ ] Exact composable `kind` set and slot shapes beyond the six named here (e.g. is `give`/`drop` needed, or does the current guardroom fixture set never require them?) — left for `/plan-draft` to pin against the existing effect vocabulary and fixture roster.
- [ ] Concrete ImGui implementation of the hint mechanism and the layout reorder — an implementation detail, not a design fork; the principle (hint not literal fill; composition precedes editor) is locked here.
- [ ] Whether the `move` conditional check is built as a general "does this exit's gating object currently block" traversal (reusable if a second room ever exists) or hardcoded to `door_north` for v0 — a plan-level tradeoff, doesn't change this spec's Design.

---

## Status Legend

| Status | Meaning |
|--------|---------|
| Draft | Still being shaped; open questions may exist |
| Ready | Open questions resolved; safe to run `/plan-draft` |
| Implemented | All linked plan phases are Done |
| Superseded | Replaced by a newer spec — link it above |
