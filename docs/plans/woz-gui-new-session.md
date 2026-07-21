# woz_gui New Session — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/woz-gui-new-session.md` |
| **Date** | 2026-07-20 |
| **Status** | In Progress (1/2 phases done) |
| **Effort** | 1 session (two phases, one file plus `.gitignore`) |

---

## 1. Overview

### Goal

After this ships, a GM can start a fresh session in `woz_gui` — reloading the same room file, resetting `turn_number`/`session_over`/`log_entries`, and clearing every piece of session-scoped editor state — in one always-clickable button, without killing and relaunching the process. Every session (including the very first one at startup) writes to its own uniquely-named log file, so no two sessions ever share a `turn_number: 1` entry in the same file. A failed room-file reload refuses cleanly, leaving the current session untouched, and surfaces its own inline error separate from the WorkOrder panel's existing `parse_error`.

### Approach

- Everything lives in `src/woz_gui.cpp` plus a one-line `.gitignore` glob update — no other file in the pipeline changes (spec §"How It Fits Existing Systems").
- The per-session log path becomes a generated `std::string` (millisecond-timestamp-suffixed), computed once at startup and regenerated only on a *successful* New Session reload — replacing the single fixed `kGuiLogPath` constant. `process_turn`'s signature already takes `const std::string&` (`gm_turn.hpp:36`), so threading the new variable through is a straight substitution, not a call-shape change.
- New Session reuses the exact `parse_world(slurp(room_path))` call the startup path already makes, and the same "parse into a local first, only commit on success" shape Submit's and Save's handlers already use — reload-then-reset is one atomic commit on success; a failure touches only the new `room_reload_error` variable and nothing else.
- `room_reload_error` is a second `std::optional<std::string>`, same type and same red-text rendering idiom as `parse_error`, but rendered in the Room Description panel (where the button lives) rather than the WorkOrder panel (where `parse_error` is rendered) — kept as two variables so a Submit/Save error in one frame can never clobber a reload error from another, or vice versa.

### Scope

**In Scope:**
- A "New Session" button in the Room Description panel, always enabled, resetting `world`/`turn_number`/`session_over`/`log_entries`/`selected_entry`/`selected_session_turn`/the editable WorkOrder buffer/the Save filename field/`parse_error` in one click (spec §2, bullets 1–2).
- `selected_saved_file` and its preview left completely untouched by New Session (spec §2, bullet 4).
- A per-session generated log path (millisecond-timestamp suffix), computed at startup and regenerated only on a successful New Session reload (spec §2, bullet 3).
- Reload-failure handling: a distinct `room_reload_error` variable, inline-rendered in the Room Description panel, leaving the current session completely untouched on failure (spec §2, bullet 5).
- Room Description panel render-order restructuring: button, then session-over banner, then reload error, then the existing room-state scroll child.
- `.gitignore` glob update for the new generated filename pattern.

**Out of Scope** (pulled from spec §3, Non-Goals):
- A file-picker widget to reload a *different* room file — New Session only ever re-parses the launch-time `room_path`.
- A confirmation modal before wiping the session — matches the existing "no confirm-modal popup pattern" decision from `woz-gui-qol.md`.
- Archiving, rotating, or cleaning up old generated session log files.
- Any change to `process_turn`/gauntlet/applier/roll semantics — only which `World`/`turn_number`/log path a turn is processed against changes.
- Any new cross-relaunch persistence (`gui_layout.json` is unaffected).
- `woz` or `woz_loop` — untouched by this plan.

---

## 2. Phased Task Breakdown

### Phase 1: Per-Session Log File Naming — Done

**Objective:** Every session — including the very first one at process startup — gets its own uniquely-named, millisecond-timestamped log file, replacing the single fixed `kGuiLogPath` constant.

#### Task 1.1: Generate a fresh, uniquely-named log path per session

**File:** `src/woz_gui.cpp`
**Tag:** T2 — timestamp/collision-avoidance logic has real correctness stakes (it's the entire mechanism behind Goal 3's "no two sessions ever share a turn 1" guarantee), worth an AI-drafted pass with author review rather than blind delegation — same tier as `woz-gui-qol.md`'s layout-persistence work.

**Changes:**
- Add `#include <chrono>` and `#include <ctime>` to the includes block (`woz_gui.cpp:9-14`).
- Remove the fixed `constexpr const char* kGuiLogPath = "gui_session_log.jsonl";` and its preceding "own session log per front end" comment (`woz_gui.cpp:36-38`) — replaced by a generator function.
- Add `static std::string generate_session_log_path()`: take `std::chrono::system_clock::now()`, split into whole seconds (`std::chrono::system_clock::to_time_t`) and the sub-second millisecond remainder (`duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000`), convert the seconds to local time via `localtime_s` (matches this project's Windows/MSVC-STL toolchain — same reasoning as `woz-gui-qol.md`'s `Actual` notes on `mftransform.h`/checked-iterator gotchas on this same toolchain), and `snprintf` the result into `gui_session_log-YYYYMMDD-HHMMSS-mmm.jsonl` — the same fixed-buffer `snprintf` idiom already used throughout this file for `wo_text_buf`/`save_filename_buf`.
- In `main()`, add `std::string log_path = generate_session_log_path();` alongside the rest of the per-session state (`woz_gui.cpp:297-310` block, next to `turn_number`/`wo_text`/`session_over`) — computed once at startup, covering the very first session per Goal 3.
- Replace the Submit handler's `turn::process_turn(world, turn_number, wo, kGuiLogPath)` (`woz_gui.cpp:490`) with `turn::process_turn(world, turn_number, wo, log_path)`.
- Update the adjacent comment referencing `kGuiLogPath` (`woz_gui.cpp:493`) to say `log_path` instead, so it doesn't describe a variable that no longer exists.

**Acceptance Criteria:**
- On startup, submitting a turn creates a log file matching `gui_session_log-\d{8}-\d{6}-\d{3}\.jsonl`, not the literal `gui_session_log.jsonl`.
- Two `generate_session_log_path()` calls made less than 1ms apart (e.g. called twice back-to-back in a scratch test loop) produce two distinct strings, differing at least in the millisecond field.
- Existing turn-processing behavior (gauntlet/applier/roll, log entry content) is unchanged — only the destination filename differs.

#### Task 1.2: `.gitignore` glob update

**File:** `.gitignore`
**Tag:** T4 — one-line change, no logic.

**Changes:**
- Replace the exact-match `gui_session_log.jsonl` line (`.gitignore:14`) with the glob `gui_session_log*.jsonl`.

**Acceptance Criteria:**
- `git status` stays clean after running `woz_gui` and submitting a turn (which creates a generated `gui_session_log-*.jsonl` file) — the file is created but never shows as untracked.

**Rollback:** git revert the phase's commit(s).

---

### Phase 2: New Session Button — Not Started

**Objective:** A GM can reset to a fresh session at any time, in one click, with the current session left completely untouched if the room file fails to reload.

#### Task 2.1: `room_reload_error` state + New Session click handler

**File:** `src/woz_gui.cpp` (uses Phase 1's `generate_session_log_path()`)
**Tag:** T2 — the reset touches roughly nine variables plus a reload-before-commit ordering with real correctness stakes (a bug here either loses the current session on a transient failure, or leaves stale state after an otherwise-successful reset) — worth an AI-drafted pass with author review, not mechanical wiring.

**Changes:**
- Add `std::optional<std::string> room_reload_error;` alongside `parse_error` in the per-session state block (`woz_gui.cpp:299` pattern) — same type and rendering idiom as `parse_error`, but a distinct variable, per the spec's decision to keep the two error sources from ever clobbering each other.
- Implement the click handler (exact render placement resolved in Task 2.2): reload `room_path` into a local `World` first, via the same `parse_world(slurp(room_path))` call the startup path already makes (`woz_gui.cpp:224`). Only on success, commit the full reset in one shot:
  - `world = reloaded; turn_number = 1; session_over = false;`
  - `log_entries.clear(); selected_entry = -1;`
  - capture `had_session_selection = selected_session_turn >= 0` *before* clearing it, then `selected_session_turn = -1;` — leave `selected_saved_file` and its indexing completely unassigned (no line touches it at all), per Goal 4.
  - `if (had_session_selection) { wo_preview_text.clear(); }` — conditional, not unconditional (spec's `wo_preview_text` decision); a Saved-entry preview is never cleared by this.
  - `wo_text = kBlankWorkOrderSkeleton;` then resync `wo_text_buf` via the same `snprintf` pattern Submit's own reset already uses (`woz_gui.cpp:502-503`).
  - `save_filename.clear(); save_filename_buf[0] = '\0';` — same clear pattern Save's success path already uses (`woz_gui.cpp:548-549`).
  - `parse_error.reset();`
  - `log_path = generate_session_log_path();`
  - `room_reload_error.reset();`
  - On a caught `std::exception`: set only `room_reload_error = ex.what();` — every variable above is left exactly as it was (matches Submit's own "parse before mutate" catch shape at `woz_gui.cpp:508-510`, just writing to a different error variable).

**Acceptance Criteria:**
- Clicking New Session while the session-over banner is showing clears the banner, re-enables Submit, and both the Log panel's entry list and the WorkOrder panel's This-Session group are empty immediately after.
- Clicking New Session mid-session (no session-over active) immediately replaces the Room Description panel's displayed world-state text with the freshly-reloaded room's.
- Given a typed draft, a Save filename, a selected This-Session entry (with its preview showing), and a stale `parse_error`, all present beforehand: after clicking New Session, the editor is blank, the filename field is empty, nothing is selected and no preview text shows in the WorkOrder panel's This Session group, and no error banner shows.
- With a Saved entry selected and previewed beforehand, clicking New Session leaves that selection and its preview exactly as they were.
- Clicking New Session twice in one run produces two distinct log files, each independently replayable from turn 1 via `gm_log.hpp`'s existing replay check.
- Hand-corrupting the room file on disk, then clicking New Session, shows an inline error and leaves `world`/`turn_number`/`log_entries`/`log_path` exactly as they were before the click; a second click with the file still broken shows the same error again; fixing the file on disk and clicking again then succeeds and clears the error.

#### Task 2.2: Room Description panel render order

**File:** `src/woz_gui.cpp` (restructures the block Task 2.1's handler attaches to)
**Tag:** T3 — mechanical reordering/insertion of render calls into an established `Begin`/`End` block, following the spec's explicit fixed order; touches a working file so reviewed rather than delegated outright.

**Changes:**
- Restructure the Room Description panel body (`woz_gui.cpp:387-395`) into, top to bottom: the "New Session" button first (unconditionally visible and unconditionally enabled — no `BeginDisabled`, unlike Submit); then the existing `if (session_over) { ... Separator(); }` block, unchanged; then, if `room_reload_error` is set, `ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", room_reload_error->c_str());` (same red-text idiom as `parse_error`'s existing render at `woz_gui.cpp:558-560`); then the existing `##RoomDescriptionScroll` child window, unchanged.

**Acceptance Criteria:**
- The New Session button renders above the session-over banner and is never pushed offscreen by it.
- `room_reload_error`'s inline text renders in the Room Description panel (not the WorkOrder panel), directly below the session-over banner (or directly below the button if no banner is showing) and above the room-state scroll child.
- With neither the banner nor the reload error showing, the panel is visually identical to today except for the added button at the top.

**Rollback:** git revert the phase's commit(s).

---

## 3. Files to Modify

| Path | Purpose | Est. LOC | Risk |
|------|---------|----------|------|
| `src/woz_gui.cpp` | Per-session log path generator, New Session button + reset/reload-failure handler, Room Description panel render order | +90/-6 | Medium |
| `.gitignore` | `gui_session_log.jsonl` exact-match → `gui_session_log*.jsonl` glob | +1/-1 | Low |

---

## 4. Validation Checklist

### Functional
- [ ] New Session resets `world`/`turn_number`/`session_over`/`log_entries` in one click, at any time, not gated by `session_over` (Phase 2).
- [ ] The editable WorkOrder buffer, Save filename field, This-Session selection/preview, and any stale `parse_error` are all cleared alongside (Phase 2).
- [ ] Each session — including the first, at startup — writes to its own uniquely-named log file; two New Session clicks in one run never collide (Phase 1 + 2).
- [ ] The Saved group's contents, selection, and preview are completely unaffected by New Session (Phase 2).
- [ ] A failed room-file reload leaves the session untouched and shows an inline error in the Room Description panel, distinct from `parse_error` (Phase 2).

### Integration
- [ ] New Session is always enabled, never gated by `session_over`, unlike Submit (Phase 2).
- [ ] `room_reload_error` and `parse_error` remain two distinct variables and render sites, never cross-clobbering each other (Phase 2).
- [ ] `git status` stays clean after normal use — generated `gui_session_log-*.jsonl` files never show as untracked (Phase 1).

### Regression
- [ ] All five existing test suites (`test_gauntlet`, `test_log`, `test_applier`, `test_roll`, `test_turn`) pass unchanged — this plan touches no header they cover.
- [ ] `woz` and `woz_loop` remain unaffected — this plan touches only `woz_gui.cpp` and `.gitignore`.
- [ ] Submitting a valid WorkOrder still advances the turn and appends to the log exactly as before, just under the new generated filename (Phase 1).

---

## 5. Risk Summary

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| `localtime_s`'s MSVC-STL signature is reversed from POSIX `localtime_r` (`tm*` destination first, `time_t*` source second) — easy to transpose and get either a compile error or a subtly wrong local time | Low | Low | Task 1.1's acceptance criteria checks the generated filename's date/time fields visually match the system clock before Phase 2 begins |

---

## Status Legend

| Marker | Meaning |
|--------|---------|
| Not Started | Work hasn't begun |
| In Progress | Currently being implemented |
| Done | Phase implemented and verified |

Update the header `Status` field as phases complete (e.g. `In Progress (1/2 phases done)`).
