# Player Input Logging — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/player-input-logging.md` |
| **Date** | 2026-07-30 |
| **Status** | In Progress (1/2 phases done) |
| **Effort** | 1 session |

---

## 1. Overview

### Goal

Every `gm::log::LogEntry` carries a `player_input` field with the raw text the GM read/heard from the player, for every outcome kind (`applied`, `gauntlet_rejected`, `modifier_budget_rejected`); `woz_gui.cpp`'s GM WorkOrder panel gains an inline field to capture it before Submit; old log lines without the field still parse.

### Approach

- Add `player_input` to `gm::log::LogEntry` following the exact tolerant-of-omission convention `gm_log.hpp` already uses for `Applied::vital_transitions` and `GauntletRejected::conflicting_with_index` — defaults to `""` when absent on read, no schema migration.
- Thread it through `gm::turn::process_turn` (`gm_turn.hpp`) as one new parameter, into the three `LogEntry{...}` construction sites already inside that function — no new logic, same "one call = one turn = one entry" invariant.
- `woz_gui.cpp` gets one new fixed-capacity buffer (same convention as `wo_text_buf`) backing an inline `InputText` above the WorkOrder JSON editor; its value resets on the same three events `wo_text` already resets on (Submit success, Reset button, New Session).
- `woz_loop.cpp` and `wizard_of_oz_demo.cpp` only need their `process_turn` call site updated to pass `""` — no input handling added there (spec Non-Goal).

### Scope

**In Scope:** `LogEntry` schema field + serialization, `process_turn` signature/plumbing, `woz_gui.cpp` inline capture UI, test coverage in `test_log.cpp`/`test_turn.cpp`, mechanical call-site updates everywhere else `process_turn` is called.

**Out of Scope** (pulled from spec Non-Goals): `woz_loop.cpp`/`wizard_of_oz_demo.cpp` input UI; required/validated player_input on Submit; persisting player_input into `workorders/*.json` fixtures; deriving `interpretation` from `player_input` or vice versa; a standalone Player Input panel; backfilling old session logs.

---

## 2. Phased Task Breakdown

### Phase 1: Schema + engine plumbing — Done

**Objective:** `player_input` exists on `LogEntry`, flows through `process_turn`, and is covered by tests, with every non-GUI call site updated to compile against the new signature.

#### Task 1.1: Add `player_input` to `gm::log::LogEntry`

**File:** `src/gm_log.hpp`
**Tag:** T3 — mechanical field addition copying an exact existing pattern (`conflicting_with_index`'s tolerant-omission read), but touches the replay log schema so the author should review every line.

**Changes:**
- Add `std::string player_input;` to the `LogEntry` struct (`gm_log.hpp:152-156`).
- In `to_json(json&, const LogEntry&)` (`gm_log.hpp:251-261`): add `j["player_input"] = e.player_input;`.
- In `from_json(const json&, LogEntry&)` (`gm_log.hpp:262-266`): `e.player_input = j.value("player_input", std::string{});` — tolerant of omission, same call shape already used for `vital_transitions`.

**Acceptance Criteria:**
- A `LogEntry` constructed with a non-empty `player_input` round-trips through `to_json`/`from_json` unchanged.
- A hand-built JSON object with no `"player_input"` key parses via `from_json` with `player_input == ""`, no exception.

#### Task 1.2: Thread `player_input` through `process_turn`

**File:** `src/gm_turn.hpp`
**Tag:** T3 — mechanical parameter threading into three pre-existing construction sites, no new branching logic.

**Changes:**
- Add a `const std::string& player_input` parameter to `process_turn` (`gm_turn.hpp:35-36`).
- Pass it into all three `log::LogEntry{turn_number, wo, outcome}` constructions inside the function (`gm_turn.hpp:43`, `72`, `80`) as the new fourth field.

**Acceptance Criteria:**
- All three outcome branches (`ModifierBudgetRejected`, `GauntletRejected`, `Applied`) log the `player_input` value passed in, verified per-branch in `test_turn.cpp`.

#### Task 1.3: Update non-GUI `process_turn` call sites

**Files:** `src/woz_loop.cpp`, `src/wizard_of_oz_demo.cpp`, `src/test_turn.cpp`
**Tag:** T4 — purely mechanical, pass `""` (or a fixture string in tests) to match the new signature; no behavioral change.

**Changes:**
- `woz_loop.cpp:117` — pass `""` as the new argument.
- `wizard_of_oz_demo.cpp:83` — pass `""` as the new argument.
- `test_turn.cpp:58, 88, 112, 115, 121` — pass `""` at all five call sites, purely to compile against the new signature; existing assertions are unaffected since none of them inspect `player_input`. Task 1.5 is the one that gives the 106-128 block's call sites real per-outcome values — don't preempt that here.

**Deviation (as implemented):** this task's file list missed `woz_gui.cpp:558`, which also calls `process_turn` and so breaks the build the moment the signature changes — Phase 1 can't compile without it. Implemented as a `""` placeholder plus a `// TODO(phase2)` marker; Phase 2 Task 2.2 replaces it with the real captured value. The two `test_turn.cpp` sites in the 106-128 block went straight to Task 1.5's real values rather than being written as `""` and immediately overwritten.

**Acceptance Criteria:**
- All three binaries build clean with the new `process_turn` signature.
- `wizard_of_oz_demo.cpp` and `woz_loop.cpp`'s console output is byte-for-byte unchanged from before this change (neither prints `player_input`).

#### Task 1.4: Extend `test_log.cpp` with `player_input` round-trip cases

**File:** `src/test_log.cpp`
**Tag:** T3 — new test block following the file's own established pattern (see case 5d for the "old log line without the field" precedent).

**Changes:**
- New case: a `LogEntry` built with a non-empty `player_input` round-trips through `to_json`/`json::parse(dump)`/`from_json` with the value preserved (mirrors the Applied round-trip in case 1).
- New case: a JSON object built without a `"player_input"` key (simulating a pre-existing log line) parses with `player_input == ""` rather than throwing — same style as case 5d's `conflicting_with_index` precedent.
- New case: an empty-string `player_input` round-trips as `""` (present in JSON, not omitted) — distinguishing "explicitly empty" from "absent," matching how `vital_transitions` distinguishes `[]` from omitted (case 9).

**Acceptance Criteria:**
- `build/bin/test_log` passes with the new checks included, `g_fail == 0`.

#### Task 1.5: Extend `test_turn.cpp` to assert `player_input` flows through

**File:** `src/test_turn.cpp`
**Tag:** T3 — small addition to existing per-outcome-kind test blocks.

**Changes:**
- In the "exactly one log line appended per call" block (`test_turn.cpp:106-128`), pass a distinct literal `player_input` string per call and assert `log::read_log(kScratchLog)`'s corresponding entry carries that exact string, for all three outcome kinds (Applied, GauntletRejected, ModifierBudgetRejected).

**Acceptance Criteria:**
- `build/bin/test_turn` passes with the new checks included, `g_fail == 0`.

**Rollback:** `git revert` the commit(s) for this phase.

---

### Phase 2: `woz_gui.cpp` inline capture field — Not Started

**Objective:** The GM WorkOrder panel has a working inline field that captures the player's raw words and carries them into the logged entry, with a lifecycle matching `wo_text`'s existing reset points.

#### Task 2.1: Add the inline player-input field to the GM WorkOrder panel

**File:** `src/woz_gui.cpp`
**Tag:** T2 — the spec left exact placement/label as an open (cosmetic) question; a real small design call the author should weigh in on, not pure mechanics.

**Changes:**
- Add `std::string player_input;` and a fixed-capacity `char player_input_buf[...]` alongside the existing `wo_text`/`wo_text_buf` declarations (`woz_gui.cpp:311, 334-335`) — size can be much smaller than `kWorkOrderBufSize` (this is a short utterance, not a JSON document); introduce a `kPlayerInputBufSize` constant near `kWorkOrderBufSize` (`woz_gui.cpp:58`).
- Render an `ImGui::InputText("Player said", player_input_buf, sizeof(player_input_buf))` (or `InputTextMultiline` for a couple of lines) above the `##WorkOrderPreview`/`##WorkOrderEditor` split in the GM WorkOrder panel (`woz_gui.cpp:524` area), reading `player_input = player_input_buf;` once per frame, same pattern as `wo_text`.

**Acceptance Criteria:**
- The field renders in the GM WorkOrder panel and accepts free text without affecting the WorkOrder JSON editor below it.
- Leaving it empty does not disable or block the Submit button (spec Non-Goal: optional field).

#### Task 2.2: Wire the field into Submit / Reset / New Session

**File:** `src/woz_gui.cpp`
**Tag:** T3 — mechanical wiring into three pre-existing reset points, following the exact `wo_text` lifecycle already coded at each site.

**Changes:**
- Submit handler (`woz_gui.cpp:555-579`): replace the `""` placeholder Phase 1 left at the `turn::process_turn` call (marked `// TODO(phase2)`) with `player_input`, and add it to the `log_entries.push_back(nlohmann::json(log::LogEntry{...}))` reconstruction — that aggregate currently omits the 4th member, so the GUI's in-memory copy would silently show an empty `player_input` while the on-disk line has the real one; on success, reset `player_input`/`player_input_buf` the same way `wo_text`/`wo_text_buf` reset.
- Reset button (`woz_gui.cpp:585-588`): also clear `player_input`/`player_input_buf`.
- New Session handler (`woz_gui.cpp:436-437`): also clear `player_input`/`player_input_buf`.

**Acceptance Criteria:**
- Submitting a turn with non-empty player-input text produces a log entry (both the in-memory `log_entries` copy and the on-disk JSONL line) whose `player_input` matches what was typed, verified by inspecting a session log file after a manual run.
- After a successful Submit, after pressing Reset, and after New Session, the field is visibly blank.
- A failed Submit (parse error) leaves the player-input field's text untouched, matching `wo_text`'s existing "left as-is on error" behavior (`woz_gui.cpp:577`).

**Rollback:** `git revert` the commit(s) for this phase.

---

## 3. Files to Modify

| Path | Purpose | Est. LOC | Risk |
|------|---------|----------|------|
| `src/gm_log.hpp` | Add `player_input` field + serialization | +6 | Low |
| `src/gm_turn.hpp` | Thread `player_input` param into 3 `LogEntry` sites | +4 | Low |
| `src/woz_loop.cpp` | Call-site update (pass `""`) | +1 | Low |
| `src/wizard_of_oz_demo.cpp` | Call-site update (pass `""`) | +1 | Low |
| `src/test_turn.cpp` | Call-site updates + new per-outcome assertions | +15 | Low |
| `src/test_log.cpp` | New round-trip test cases | +30 | Low |
| `src/woz_gui.cpp` | New buffer, inline field, Submit/Reset/New Session wiring | +20 | Medium — touches the panel's most-edited region (Submit handler) |

---

## 4. Validation Checklist

### Functional
- [ ] Submitting a turn in `woz_gui.cpp` with non-empty player-input text produces a log entry (in-memory and on-disk) whose `player_input` matches what was typed.
- [ ] Submitting with an empty player-input field logs `player_input` as `""`, and Submit is not blocked.

### Integration
- [ ] `woz_loop.cpp` and `wizard_of_oz_demo.cpp` build and run with console output unchanged.
- [ ] The Log panel's detail pane (raw JSON dump, `woz_gui.cpp:655`) shows `player_input` for a newly logged entry with no additional UI code — confirms the existing full-entry-dump display picks it up automatically.

### Regression
- [ ] All pre-existing `test_log.cpp` and `test_turn.cpp` checks still pass unmodified.
- [ ] A log line written before this field existed (an old session's `.jsonl` file, or a hand-built JSON object omitting the key) still parses via `read_log`/`from_json` without throwing.

---

## Status Legend

| Marker | Meaning |
|--------|---------|
| Not Started | Work hasn't begun |
| In Progress | Currently being implemented |
| Done | Phase implemented and verified |

Update the header `Status` field as phases complete (e.g. `In Progress (2/4 phases done)`). `/plan-update` does this automatically.
