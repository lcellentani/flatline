# Player Input Logging — SPEC

| Field | Value |
|-------|-------|
| **Status** | Implemented |
| **Date** | 2026-07-30 |
| **Plan** | `docs/plans/player-input-logging.md` (Done — 2/2 phases done) |
| **Supersedes** | N/A |

---

## 1. Problem

The replay log stores the `WorkOrder`'s `interpretation` field as the only record of player intent, but `interpretation` is the GM's paraphrase, authored after the fact — never the player's own words. `docs/context/investigation-plan-experiment-2.md` §6 (Immediate Action #1) needs `(pre-state, player's raw words)` pairs to harvest Experiment-1 replay logs as Experiment-2 benchmark scenarios (DD-7); a paraphrase is a lossy substitute for that. A dedicated Player Input panel was considered during Phase 3 exec planning and cut (`docs/exec/phase-3-woz-gui.md`) specifically because it had "no schema field, no persistence, and no logging" — a scratchpad whose entire content just got manually retyped into `interpretation` a few inches away. That gap is what this spec closes: give the raw utterance a schema field, persist it, and log it, so it stops being retyped and starts being captured.

## 2. Goals

- Every `gm::log::LogEntry` gained a `player_input` field carrying the raw text the GM read/heard from the player before authoring the WorkOrder, for **every** outcome kind (`applied`, `gauntlet_rejected`, `modifier_budget_rejected`) — rejected turns are still valid benchmark scenarios (a player attempt that failed the gauntlet is exactly the kind of case DD-7 wants).
- `woz_gui.cpp`'s GM WorkOrder panel has an inline text field where the GM transcribes the player's words before authoring the WorkOrder JSON below it; submitting a turn carries that text into the resulting log entry.
- Old log lines written before this field existed still parse (`from_json` tolerant of omission, same convention as `vital_transitions` and `conflicting_with_index` in `gm_log.hpp`).
- `test_log.cpp` and `test_turn.cpp` cover round-tripping `player_input` (present, empty, and absent-on-old-lines) through `to_json`/`from_json` and through `process_turn`.

## 3. Non-Goals

- **`woz_loop.cpp` and `wizard_of_oz_demo.cpp` UI/input changes** — neither is used for live Experiment-1 sessions (per `CLAUDE.md`, `woz_gui.cpp` is); they keep calling `process_turn` with an empty/absent `player_input`. The schema field is still shared by all three since they all go through `gm_log.hpp`/`gm_turn.hpp`.
- **Required/validated player_input on Submit** — the field is optional; an empty string logs as an empty string. Forcing transcription before every submit adds live-play friction for no clear payoff (e.g. loading a Saved fixture workorder with no live utterance behind it).
- **Persisting `player_input` into the `workorders/*.json` Saved-fixture files** — those are curated design-time templates, not live-session records; this spec only touches the JSONL replay log.
- **Replacing or deriving `interpretation` from `player_input`, or vice versa** — the two coexist. `interpretation` remains the GM's paraphrase (used for gauntlet reasoning and narration); `player_input` is the independent raw source. Neither is computed from the other.
- **A standalone Player Input panel / new dock node** — the field is inline in the existing GM WorkOrder panel, not a new panel (revisits, but does not reverse, the Phase 3 cut: the capability is added without reinstating the separate scratchpad UI).
- **Retroactively backfilling `player_input` into already-recorded session logs** — out of reach by construction; those turns' raw player words were never captured, and reconstructing them from `interpretation` would just reintroduce the lossy paraphrase this spec exists to stop relying on.

## 4. Design

### Key Decisions

- **`player_input` lives on `gm::log::LogEntry`, not on `WorkOrder`** — it's session/log metadata about how a turn was produced, not a field the gauntlet, roll, or applier ever reason about. Keeping it off `WorkOrder` means `gm_roll.hpp`'s seed material (`turn_number` + WorkOrder JSON dump) is unaffected, and the engine's closed vocabulary stays untouched. Alternative considered: adding it to `WorkOrder` directly — rejected because it would perturb the replay seed and blur the "engine vocabulary vs. session record" boundary the adjudicator contract draws.
- **Always written, tolerant of omission on read** — the two halves follow different existing precedents in `gm_log.hpp`, deliberately: on *write* it is always emitted, empty string included, like `Applied::vital_transitions`' `[]` (not omitted-when-absent like `GauntletRejected::conflicting_with_index`); on *read* `from_json` defaults to `""` when the key is missing. This is what lets a benchmark reader tell "the turn was captured and the player said nothing recorded" (`"player_input": ""`) apart from "this line predates the field" (key absent) — a distinction DD-7's scenario harvesting needs, and one that an omit-when-empty write would erase. Old log lines keep parsing without a schema migration.
- **`process_turn` gains a `player_input` parameter** (`gm_turn.hpp`), threaded straight into the `LogEntry` it already builds and appends for every outcome branch (`applied`, `gauntlet_rejected`, `modifier_budget_rejected` — three call sites in that function today). All three front ends' call sites (`woz_gui.cpp`, `woz_loop.cpp`, `wizard_of_oz_demo.cpp`) and `test_turn.cpp` need the new argument; only `woz_gui.cpp` passes a live value, the rest pass an empty string.
- **Inline field, not a new panel** — a single-line (or short multiline) `InputText` above the WorkOrder JSON editor in the existing GM WorkOrder panel. Alternative considered: reinstating the separate Player Input panel Phase 3 cut — rejected because the cut's stated reason (no schema/persistence/logging) is what this spec fixes, but the panel itself was never the point; adding a second dock node is more UI surface than the capability needs.
- **Field resets alongside `wo_text` on Submit and on Room reload** — same lifecycle as the existing `wo_text`/`kBlankWorkOrderSkeleton` reset behavior (`woz_gui.cpp:436, 570, 586`), so a stale player utterance can't linger into the next turn's log entry by accident.

### How It Fits Existing Systems

- `gm_log.hpp` — `LogEntry` struct gains the field; its `to_json`/`from_json` pair is extended following the same flattened-object, tolerant-of-omission pattern already used there.
- `gm_turn.hpp` — `process_turn`'s signature grows one parameter, threaded into all three `LogEntry{...}` construction sites inside it.
- `woz_gui.cpp` — a new `char` buffer (same fixed-capacity-buffer convention as `wo_text_buf`) backing an `InputText` in the GM WorkOrder panel; its value is read once per frame and passed to `process_turn` at Submit time, then reset.
- `test_log.cpp` / `test_turn.cpp` — extended with round-trip cases for the new field (present, empty, absent-on-old-lines).
- `woz_loop.cpp`, `wizard_of_oz_demo.cpp` — call-site signature update only (pass an empty string); no behavioral change, no new input handling.

## 5. Open Questions

- [x] Exact field placement and label text in the GM WorkOrder panel — resolved in Phase 2 Task 2.1: a single-line `InputText` labeled "Player action:", inline above the WorkOrder JSON editor.

---

## Status Legend

| Status | Meaning |
|--------|---------|
| Draft | Still being shaped; open questions may exist |
| Ready | Open questions resolved; safe to run `/plan-draft` |
| Implemented | All linked plan phases are Done |
| Superseded | Replaced by a newer spec — link it above |
