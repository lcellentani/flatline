# woz_gui Orchestrator Flow — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/woz-gui-orchestrator-flow.md` |
| **Date** | 2026-08-02 |
| **Status** | In Progress (2/4 phases done) |
| **Effort** | 3–4 sessions |

---

## 1. Overview

### Goal

The GM WorkOrder panel in `woz_gui.cpp` gains a structured `kind` selector (`examine`, `move`, `interact`, `use_item_on`, `attack`, `say`) with contextual target/item/direction fields sourced from the loaded `World`. `examine` and passable `move` resolve inline as logged `Observed` beats with no WorkOrder involved; everything else composes a hint line into the existing `player_input` field and proceeds to a WorkOrder as today. The session JSONL log — read start to end — reconstructs the whole playthrough, exploratory beats included, in one shared turn-number sequence.

### Approach

- Extend `gm::log::Outcome` with a new `Observed` alternative (`kind`, `target`, `narration`) and make `LogEntry.work_order` optional, following the exact tag-on-`"status"` / tolerant-of-omission patterns `gm_log.hpp` already uses for its other three outcomes and for `player_input`/`conflicting_with_index`.
- Add a small, pure Tier-0 classification function to `woz_gui.cpp` encoding the spec's locked per-`kind` rule table, plus a general (not hardcoded) exit-gating-object traversal for `move`'s conditional case — no such traversal exists in the codebase today.
- Build the compositor as new `woz_gui.cpp` UI state/controls reading the already-loaded `World` (objects, entities, player, room exits) — no schema or engine changes; `gm_state.hpp`, `gm_turn.hpp`, `gm_gauntlet.hpp`/`gm_roll.hpp`/`gm_applier.hpp` stay untouched, matching the spec's Non-Goals.
- Tier-0 resolutions log directly via `log::append_to_file`, bypassing `process_turn` entirely, sharing its `turn_number` counter (increment after append, same as the adjudicated path).
- WorkOrder-bound compositions populate `player_input` via `ImGui::InputTextWithHint` (hint only, buffer untouched), preserving `player-input-logging`'s "empty ⇒ no raw input captured" invariant.
- `woz_loop.cpp`, `wizard_of_oz_demo.cpp`, and `gm_turn.hpp` need no changes: they always construct a real `WorkOrder`, which converts implicitly into the now-optional field.

### Scope

**In Scope:** `gm::log::Outcome`/`LogEntry` schema + serialization changes, `test_log.cpp` coverage, the `woz_gui.cpp` compositor UI (kind selector, contextual fields, inline Tier-0 resolution, Tier-0 logging), hint-based `player_input` population, and the panel layout reorder.

**Out of Scope** (pulled from spec Non-Goals): a stored player-input example table; resolving `gm-adjudicator.md`'s trigger-boundary question or building a real movement system; any LLM/inference involvement; changes to the six-primitive effect vocabulary, `gm_gauntlet.hpp`, `gm_roll.hpp`, or `gm_applier.hpp`; `woz_loop.cpp`/`wizard_of_oz_demo.cpp` changes; a general-purpose multi-room compositor; a `give`/`drop` kind (spec Key Decision — no clean vocabulary mapping today).

---

## 2. Phased Task Breakdown

### Phase 1: Log schema — `Observed` outcome + optional `work_order` — Done

**Objective:** `gm::log::LogEntry` can represent a Tier-0 beat (no `WorkOrder`) alongside the three existing adjudicated outcomes, with full JSON round-trip coverage.

#### Task 1.1: Add `Observed` alternative to `Outcome` — Done

**File:** `src/gm_log.hpp`
**Tag:** T3 — mirrors the existing `ModifierBudgetRejected` tag-on-status shape exactly, but touches the replay log schema so the author should review every line.

**Changes:**
- Add `struct Observed { std::string kind; std::string target; std::string narration; };` near the other `Outcome` alternatives (`gm_log.hpp:118-148`).
- Add `Observed` to the variant: `using Outcome = std::variant<ModifierBudgetRejected, GauntletRejected, Applied, Observed>;` (`gm_log.hpp:150`).
- `to_json(json&, const Observed&)`: `j["status"] = "observed";` then flatten `kind`/`target`/`narration` onto `j`, following `ModifierBudgetRejected`'s `to_json` shape (`gm_log.hpp:181` area).
- `from_json(const json&, Observed&)`: three required-field reads, no optional sub-fields.
- `Outcome::from_json` (`gm_log.hpp:244-254`): add an `else if (status == "observed") { o = j.get<Observed>(); }` branch before the unknown-tag `throw`.

**Acceptance Criteria:**
- A `LogEntry` wrapping an `Observed{"examine", "guard_01", "..."}` round-trips through `to_json`/`from_json` with all three fields preserved and `status == "observed"`.
- An unrecognized `"status"` value still throws `std::runtime_error` (regression — the new branch doesn't loosen the existing check).

#### Task 1.2: Make `LogEntry.work_order` optional — Done

**File:** `src/gm_log.hpp`
**Tag:** T2 — changes the implicit contract of every existing deserialization call site (a `WorkOrder` used to be guaranteed present); a real judgment call on the tolerant-of-omission shape, not pure mechanics.

**Changes:**
- Change `WorkOrder work_order;` to `std::optional<WorkOrder> work_order;` on `LogEntry` (`gm_log.hpp:~155`).
- `LogEntry::to_json` (`gm_log.hpp:259-274`): only emit the `"work_order"` key when the optional has a value, following `conflicting_with_index`'s `if`-guarded emission pattern (`if (o.conflicting_with_index) j["conflicting_with_index"] = *o.conflicting_with_index;`, `gm_log.hpp:200`).
- `LogEntry::from_json` (`gm_log.hpp:~278-279`): replace the current hard `j.at("work_order").get_to(e.work_order)` with a `j.contains("work_order")` check that assigns the parsed value or leaves `e.work_order` as `std::nullopt`.

**Acceptance Criteria:**
- A `LogEntry` built with `work_order = std::nullopt` round-trips with no `"work_order"` key in the dumped JSON and `work_order == std::nullopt` after reparsing.
- A `LogEntry` built with a real `WorkOrder` still round-trips unchanged (regression against the three existing outcome kinds).
- A hand-built JSON object with a `"work_order"` key present (simulating an old on-disk log line) still parses with `work_order` populated.
- The full project builds clean against the new `std::optional<WorkOrder>` member with no call-site edits elsewhere: `gm_turn.hpp`'s three `LogEntry{...}` construction sites and `woz_gui.cpp`'s own `LogEntry{turn_number, wo, outcome, player_input}` reconstruction (`woz_gui.cpp:578`) all still compile as-is, since `std::optional<WorkOrder>` converts implicitly from a plain `WorkOrder` in aggregate-init — verified at Phase 1, not deferred to the plan-end Validation Checklist.

#### Task 1.3: Extend `test_log.cpp` with `Observed` and `work_order`-absent coverage — Done

**File:** `src/test_log.cpp`
**Tag:** T3 — new test blocks copying two existing precedents exactly: Section 4's hand-built-no-pipeline `ModifierBudgetRejected` shape, and Section 10c's tolerant-of-omission `j.erase(...)` shape.

**Changes:**
- New section: hand-build `log::Observed{...}`, wrap in `LogEntry{n, std::nullopt, observed, ""}`, dump, reparse, assert fields survive and `status == "observed"` (mirrors Section 4).
- New section: take an existing `Applied`-outcome `LogEntry`'s dumped JSON, `j.erase("work_order")`, reparse, assert `work_order == std::nullopt` and the rest of the entry is unaffected (mirrors Section 10c's `player_input`-omission case).

**Acceptance Criteria:**
- `build/bin/test_log` passes with the new checks included, `g_fail == 0`.

**Rollback:** `git revert` the commit(s) for this phase.

---

### Phase 2: Tier-0 detector — classification + exit traversal — Done

**Objective:** A pure, spec-locked classification function determines whether a composed `kind`+target resolves Tier-0 or routes to WorkOrder, with no such logic existing in the codebase yet.

**Exec-planning revision (locked before implementation started):** `exit_permits_passage`/`is_tier0` live in a new header, `src/woz_gui_detector.hpp`, not as `static` functions inside `woz_gui.cpp` as originally drafted — so a real `test_woz_gui_detector.cpp` binary can link against them like the project's other five `test_*.cpp` targets, rather than relying on a code-read-only check with no automated coverage until Phase 3's GUI exists. `woz_gui.cpp` gains an `#include "woz_gui_detector.hpp"` once Phase 3 actually calls into it; Phase 2 itself doesn't touch `woz_gui.cpp`. This adds Task 2.3 and a `CMakeLists.txt` entry not in the original plan draft.

#### Task 2.1: Exit-gating-object traversal helper — Done

**File:** `src/woz_gui_detector.hpp` (new)
**Tag:** T2 — no existing precedent to copy (confirmed by search); the lookup itself is simple but the "how do I structure this against `World`" call is a real small design decision.

**Changes:**
- New function, e.g. `bool exit_permits_passage(const World& world, const std::string& direction)`: looks up `world.room.exits` for `direction`, finds the matching `ObjectState` in `world.objects` by id, reads its `"locked"` property via `std::get_if<bool>`, returns `!locked`. Missing exit, missing object, or a non-bool/absent `"locked"` property all default to blocking (`false`) — the detector only ever decides bypass-or-not, never legality, so the safe default is "needs the GM."
- Object-by-id lookup follows the existing raw-`for`-loop-with-pointer idiom already used twice in the codebase (`gm_rules.hpp`'s `object_exists`, `gm_gauntlet.hpp`'s `preconditions(SetObjectState)`), not a new lookup style.

**Acceptance Criteria:**
- Against `room_guardroom.json` (`door_north`, `locked: true`), returns `false`.
- Against a hand-edited local copy with `locked: false`, returns `true`.
- Against a nonexistent direction, returns `false` without throwing.

#### Task 2.2: Per-`kind` Tier-0 classification function — Done

**File:** `src/woz_gui_detector.hpp`
**Tag:** T1 — this function is a direct encoding of the spec's locked per-`kind` rule table (§4 Key Decisions); the author should write it by hand since a subtle bug here changes what does or doesn't get GM review.

**Changes:**
- New function, e.g. `bool is_tier0(const std::string& kind, const World& world, const std::string& target)`: `examine` → always `true`; `move` → `exit_permits_passage(world, target)`; `interact`/`use_item_on`/`attack`/`say` → always `false`; any other/garbage `kind` string → `false` (silent safe-default, consistent with the rest of the detector's philosophy — unreachable from the real UI today since Task 3.1 constrains `compositor_kind` to the six literals via a fixed combo, but the function's own contract still needs to be total).

**Acceptance Criteria:**
- Automated, via Task 2.3's `test_woz_gui_detector.cpp` against hand-constructed `World` fixtures (no running GUI needed): each of the six kind literals passed to `is_tier0` returns the value the spec's rule table dictates — `examine` always `true`; `interact`/`use_item_on`/`attack`/`say` always `false`; `move` matching `exit_permits_passage`'s return for both an open and a blocked exit. A garbage kind string returns `false`. The full interactive truth-table check against the running GUI is still deferred to Phase 3's Task 3.3 acceptance criteria as a regression check, once the compositor UI exists to exercise it.

#### Task 2.3: `test_woz_gui_detector.cpp` + CMake target — Done

**File:** `src/test_woz_gui_detector.cpp` (new), `CMakeLists.txt`
**Tag:** T3 — mechanical: copies `test_log.cpp`'s harness shape (`check()`/`g_pass`/`g_fail`, hand-built `World` fixtures) and `CMakeLists.txt`'s existing `flatline_add_executable(test_X src/test_X.cpp)` pattern verbatim.

**Actual:** T2 — the `exit_permits_passage`/CMake-target commit was tagged T2, not T3; the author requested Phase 2 commits be finalized at T2 rather than following each task's individually reasoned tag. The later `is_tier0` test coverage was folded into Task 2.2's T1 commit.

**Changes:**
- New standalone test executable exercising Task 2.1/2.2's acceptance criteria directly: `room_guardroom.json`-shaped fixture plus a hand-mutated `locked: false` copy for `exit_permits_passage`; all six `kind` literals plus one garbage string for `is_tier0`.
- `CMakeLists.txt`: add `flatline_add_executable(test_woz_gui_detector src/test_woz_gui_detector.cpp)` alongside the other `test_*` targets (`CMakeLists.txt:49-53`).

**Acceptance Criteria:**
- `build/bin/test_woz_gui_detector` passes with `g_fail == 0`, covering every case listed in Task 2.1/2.2's acceptance criteria.

**Rollback:** `git revert` the commit(s) for this phase.

---

### Phase 3: Compositor UI — kind selector, contextual fields, inline resolution — Not Started

**Objective:** The GM WorkOrder panel has a working structured compositor: pick a `kind`, pick a target/item/direction from the real `World`, and see `examine`/open-`move` resolve inline.

#### Task 3.1: Compositor state + kind selector

**File:** `src/woz_gui.cpp`
**Tag:** T2 — new persistent UI state and its reset lifecycle (what clears on Submit/Reset/New Session, mirroring `wo_text`'s existing lifecycle) is a real design call, not pure mechanics.

**Changes:**
- New state declared near `wo_text`/`player_input` (`woz_gui.cpp:311-335` area): a `compositor_kind` index into `{"examine","move","interact","use_item_on","attack","say"}`, plus per-kind target/item/direction selection state.
- Render an `ImGui::Combo("Action", &compositor_kind, ...)` as a new compositor section, inserted between the preview pane's `Separator()` (`woz_gui.cpp:554`) and the current player-input line (`woz_gui.cpp:558`) — the slot the spec's layout reorder (Task 4.4) ultimately wants it in.
- Wire the new state into the same three reset points `wo_text`/`player_input` already use (Submit success, Reset button, New Session).

**Acceptance Criteria:**
- The combo renders in the panel; selecting a kind updates `compositor_kind` without affecting `wo_text`/`player_input`.
- Submit, Reset, and New Session all clear compositor state back to its default, matching `wo_text`'s existing reset behavior.

#### Task 3.2: Contextual target/item/direction fields sourced from `World`

**File:** `src/woz_gui.cpp`
**Tag:** T2 — mapping each kind to its field set is a UX judgment call, not mechanical plumbing.

**Changes:**
- `examine`: a combo enumerating `world.objects` ids + `world.entities` ids + a sentinel "(yourself)" entry for the player.
- `move`: a combo enumerating `world.room.exits` keys (directions).
- `use_item_on`: an item combo from `world.player.inventory`, plus a target combo from objects+entities.
- `interact`/`attack`: a target combo from objects+entities.
- `say`: no field, per the spec's locked decision — nothing renders beyond the kind selector.
- Switching `compositor_kind` resets the prior kind's selection state (no stale cross-kind target survives a kind switch).

**Acceptance Criteria:**
- Each kind shows only its relevant field(s).
- Against the default `room_guardroom.json`, combo contents match its actual entries: `door_north` (exit + object), `guard_01` (entity), `dagger`/`oil_flask` (player inventory).

#### Task 3.3: Inline Tier-0 resolution display — `examine` & open `move`

**File:** `src/woz_gui.cpp`
**Tag:** T2 — the description-assembly logic (which properties to read per target kind) is a real authoring decision about what the GM sees, not mechanical plumbing.

**Changes:**
- When `is_tier0(...)` is true for the current kind+target selection, render a read-only text block: for `examine` on an entity, its `hp`/`disposition`/`holds`/`persona`; on an object, its properties map; on the player, `hp`/`gold`/`inventory`; for `move`, a fixed passage-confirmation line.
- This text becomes the `narration` value Task 4.1 logs into `Observed`.

**Acceptance Criteria:**
- Examining `guard_01` shows its `hp`/`disposition`/`holds`/`persona`.
- Examining the player shows `hp`/`gold`/`inventory`.
- Composing `move` toward `door_north` (`locked: true`) does **not** show this inline block — it routes to the hint path instead (Task 4.3).
- Full interactive truth-table check against the running GUI (deferred from Task 2.2): all six kinds, plus both `move` sub-cases (open/blocked exit), resolve inline exactly where `is_tier0` says they should and nowhere else.

**Rollback:** `git revert` the commit(s) for this phase.

---

### Phase 4: Tier-0 logging, hint-based `player_input`, layout reorder — Not Started

**Objective:** Tier-0 resolutions get logged as `Observed` entries sharing the adjudicated-turn counter, WorkOrder-bound compositions populate `player_input`'s hint (not its buffer), and the panel's visual order matches explore → input → GM causality.

#### Task 4.1: Log Tier-0 resolutions as `Observed` `LogEntry`

**File:** `src/woz_gui.cpp`
**Tag:** T2 — wiring a new logging path that shares `turn_number` with the adjudicated-turn counter is exactly the "new pipeline entry point" the spec calls out as a real decision, not pure mechanics.

**Changes:**
- Add a "Log Observation" affordance, shown only when Task 3.3's inline resolution is displayed, that builds `log::Observed{kind_name, target_id, narration}`, wraps it in `log::LogEntry{turn_number, std::nullopt, observed, ""}` (`player_input` left empty — a Tier-0 beat isn't a GM-authored submission), appends to `log_entries` and to `log_path` via `log::append_to_file` (the same call `process_turn` already uses internally), then increments `turn_number` — sharing the single counter with the adjudicated path (`woz_gui.cpp:311,582`).

**Acceptance Criteria:**
- Logging an `examine` beat against `guard_01` appends a JSONL line with `status: "observed"`, the correct `turn_number`, and no `"work_order"` key.
- Two Tier-0 logs followed by one adjudicated Submit produce three consecutive `turn_number` values with no gaps or repeats.

#### Task 4.2: `entry_label()` handles the `"observed"` status

**File:** `src/woz_gui.cpp`
**Tag:** T3 — mechanical extension of an existing ternary to a third case.

**Changes:**
- `entry_label()` (`woz_gui.cpp:96-101`) currently falls back to the `"reason"` field for any non-`"applied"` status; `Observed` entries have no `"reason"` field, so extend the teaser selection to use `"narration"` for `status == "observed"` too.

**Acceptance Criteria:**
- A logged `Observed` entry's list-view teaser shows its narration text, not a blank string.

#### Task 4.3: Hint-based `player_input` for WorkOrder-bound compositions

**File:** `src/woz_gui.cpp`
**Tag:** T2 — the composed-line wording per kind (matching `docs/guides/scenario-authoring.md` §3's orchestrator-voice convention) is a real authoring decision.

**Changes:**
- For `interact`/`use_item_on`/`attack`/`move`-toward-a-blocked-exit, assemble a canonical orchestrator-voice line from kind+target+item.
- Replace the current `ImGui::InputText("##PlayerInput", player_input_buf, ...)` (`woz_gui.cpp:560`) with `ImGui::InputTextWithHint("##PlayerInput", composed_line.c_str(), player_input_buf, ...)` — hint text only, buffer untouched.
- `say` contributes no composed line (per Task 3.2) — the hint stays empty, and the field behaves exactly as it did before this spec.

**Acceptance Criteria:**
- Selecting `interact` + a target shows the composed line as greyed-out placeholder text, not in the actual buffer.
- Leaving the field untouched and submitting still logs `player_input == ""` — preserves `player-input-logging`'s "empty ⇒ no raw input captured" invariant.
- Typing over the hint works exactly as `InputText` does today (regression check).

#### Task 4.4: Layout reorder

**File:** `src/woz_gui.cpp`
**Tag:** T3 — pure ImGui call reordering; the design decision is already locked in the spec.

**Changes:**
- Arrange the compositor block (Tasks 3.1–3.3) and the player-input line (`woz_gui.cpp:558-561`) directly after the preview pane's `Separator()` (`woz_gui.cpp:554`) and before the WorkOrder JSON editor (`woz_gui.cpp:563`), replacing today's flat, simultaneous arrangement.

**Acceptance Criteria:**
- Visual top-to-bottom order: compositor → player-input hint field → WorkOrder editor → Submit/Reset/Save row.
- No functional regression: Submit/Reset/New Session/Save all still work identically to before this spec.

**Rollback:** `git revert` the commit(s) for this phase.

---

## 3. Files to Modify

| Path | Purpose | Est. LOC | Risk |
|------|---------|----------|------|
| `src/gm_log.hpp` | Add `Observed` alternative, make `work_order` optional, extend JSON (de)serialization | +40 | Medium — touches the replay log schema |
| `src/test_log.cpp` | New round-trip test cases for `Observed` and `work_order`-absent | +40 | Low |
| `src/woz_gui_detector.hpp` (new) | `exit_permits_passage` + `is_tier0` Tier-0 classification, split out for testability (exec-planning revision, Phase 2) | +30 | Low |
| `src/test_woz_gui_detector.cpp` (new) | Round-trip/truth-table coverage for `woz_gui_detector.hpp` | +40 | Low |
| `CMakeLists.txt` | Add `test_woz_gui_detector` executable target | +1 | Low |
| `src/woz_gui.cpp` | Compositor state/UI, Tier-0 detector, inline resolution, `Observed` logging, hint-based `player_input`, layout reorder | +280 | Medium — touches the panel's most-edited region |

---

## 4. Validation Checklist

### Functional
- [ ] Composing `examine` against `guard_01`/`door_north`/the player resolves inline with the right properties, no WorkOrder editor touched.
- [ ] Composing `move` toward `door_north` (locked) does not resolve inline; toward a hypothetically-unlocked exit, it does.
- [ ] Composing `interact`/`use_item_on`/`attack`/`say` (or `move` toward a blocked exit) populates `player_input`'s hint text with a composed line; the buffer stays empty until the GM types.
- [ ] Logging a Tier-0 beat appends an `Observed` JSONL line sharing the `turn_number` counter with adjudicated turns, in the correct order.

### Integration
- [ ] Existing Submit/Reset/New Session/Save flows work unchanged.
- [ ] `build/bin/test_log` passes with the new `Observed` and `work_order`-absent cases.
- [ ] `woz_loop.cpp` and `wizard_of_oz_demo.cpp` build unchanged — no call-site edits needed, per spec Non-Goal.

### Regression
- [ ] All pre-existing `test_log.cpp` checks still pass.
- [ ] An old on-disk log line (pre-this-change, real `work_order` present) still parses.
- [ ] The Log panel's detail pane (raw JSON dump) displays an `Observed` entry with no additional UI code, same as it does today for the other three outcome kinds.

---

## Status Legend

| Marker | Meaning |
|--------|---------|
| Not Started | Work hasn't begun |
| In Progress | Currently being implemented |
| Done | Phase implemented and verified |

Update the header `Status` field as phases complete (e.g. `In Progress (2/4 phases done)`). `/plan-update` does this automatically.
