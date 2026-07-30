# woz_gui QoL Improvements — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/woz-gui-qol.md` |
| **Date** | 2026-07-14 |
| **Status** | Done |
| **Effort** | 1 session (four phases, one file — Splitters is the largest phase) |

---

## 1. Overview

### Goal

After this ships, `woz_gui` closes the four friction points found running real Experiment-1 sessions (spec §1): Submit can't fire against an untouched blank WorkOrder, all three fixed-size sub-splits (WorkOrder list-sidebar, WorkOrder preview/editor, Log entry-list/detail) are drag-resizable and remember their size across relaunches, a GM can abandon a draft (Reset) or promote one to a reusable template (Save) without leaving the app, and the Saved group reflects `workorders/` as it actually is right now (Refresh), not just as it looked at startup.

### Approach

- Everything lives in `src/woz_gui.cpp` — no other file in the pipeline (`gm_turn.hpp`, `gm_log.hpp`, `gm_gauntlet.hpp`, `gm_applier.hpp`, `gm_rules.hpp`) or `CMakeLists.txt` changes (spec §4, "How It Fits Existing Systems").
- Submit's gate and Reset's gate are the same boolean (buffer trimmed-equals `kBlankWorkOrderSkeleton`), computed once and read twice — Reset has zero new comparison logic.
- Save reuses the exact `parse_work_order` call Submit's handler already makes, adds one allowlist check (`[A-Za-z0-9_-]+`) ahead of the existing collision check, and appends to `saved_workorder_files` in memory on a successful write (no restart needed to see it in the Saved group).
- All three named splitters share one persisted-ratio store (`gui_layout.json`, fraction-of-space, `kMinSplitPx = 120` floor, written on every drag-release) and one interactive drag widget, kept as two separate pieces (Tasks 3.1/3.2) rather than one: persistence is deterministic JSON I/O with a direct precedent already in the file (the `saved_workorder_files` startup scan's try/catch-and-fall-back-to-empty shape), while the drag widget is genuinely interactive and has one real correctness subtlety worth isolating — see Task 3.2. Default/fallback ratios are back-computed once from a fixed `640×400` reference (half the SDL window's `1280×800` default, per the `DockBuilder`'s 0.5/0.5 splits) — one `load_or_default()` path covers both "never saved" and "saved but broken."
- The floor clamp recomputes its pixel bound from each splitter's *current* `total_extent` every frame rather than baking in a fixed value — this is what lets the spec's "don't clamp the stored fraction" instruction (protects a dragged-to ratio from being permanently distorted by a one-off small window size) and "never reaches zero/negative size" goal (§2) both hold at once, without a drag-reversal dead zone in between. Detailed in Task 3.2.
- The WorkOrder preview/editor split is the one splitter that touches the editor's existing `ImVec2(-1.0f, -N)` negative-sizing idiom; `N` grows to `GetFrameHeightWithSpacing() * 2` to fit the new one-row Submit/Reset/Save/filename-field footer plus an unconditionally-reserved error line. This is the *only* piece of Phase 3 that needs Phase 2's Reset/Save row to already exist — persistence (3.1), the drag widget itself (3.2), and the other two splitter sites (3.3) have no such dependency and could technically be built first. This plan keeps Draft Management as Phase 2 anyway, ahead of Splitters, so that dependent task (3.4) isn't stranded mid-phase; the rest of Phase 3 doesn't strictly need the reordering, it just doesn't cost anything either.
- Refresh factors the startup-time `workorders/` scan (`fs::directory_iterator`, already in the file) into one function called from both startup and the button handler — no second, parallel scan implementation.

### Scope

**In Scope:**
- Submit-gate blank-skeleton check (spec §2, bullet 1)
- Reset and Save buttons, plus Save's filename `InputText`, validation, and Saved-group live update (spec §2, bullets 5–6)
- Manual drag-splitters at all three named sites, persisted to `gui_layout.json`, with a shared 120px minimum and back-computed defaults/fallback (spec §2, bullets 2–4)
- Refresh button and the stale-selection-clears-on-refresh behavior (spec §2, bullet 7)
- `.gitignore` entry for the new generated `gui_layout.json`

**Out of Scope** (pulled from spec §3, Non-Goals):
- Top-level three-panel dockspace resizing (already handled by ImGui docking)
- A structured/form-based WorkOrder editor — raw-JSON `InputTextMultiline` unchanged
- Any change to `process_turn`/log/turn-advance semantics
- Semantic (parsed) diffing for the Submit/Reset gate — exact trimmed text-compare only
- Persisting any UI/session state beyond the three split ratios
- Renaming, deleting, or editing-in-place an existing `workorders/*.json` file
- Any confirm-modal popup pattern
- A "New Session" button/restart capability — discussed during spec drafting, deliberately not folded in; noted here as a candidate follow-up spec, not part of this plan
- Live redraw during an OS-level window-border resize drag — found during Phase 3 manual verification (`docs/exec/woz-gui-qol-phase-3-splitters.md` §"Known Limitations"); a Win32 modal-sizing-loop stall in the SDL3+OpenGL3 shell itself (pre-existing, inherited from `docs/exec/phase-3-woz-gui.md`), unrelated to this plan's in-app splitters, which do live-update correctly. Noted here as a candidate follow-up, not part of this plan
- An automated test suite for `woz_gui.cpp`'s logic (blank-skeleton compare, filename allowlist, layout load/save/defaults) — all genuinely unit-testable as pure functions, but a new `test_*` binary would require a `CMakeLists.txt` change, which the spec's own "How It Fits Existing Systems" explicitly rules out. Verification here stays manual, same as Task 3.4 in `gm-multiturn-loop.md`. Worth reconsidering in a future spec if `woz_gui.cpp` keeps growing.

---

## 2. Phased Task Breakdown

### Phase 1: Submit Gate — Done

**Objective:** "Submit Turn" is disabled whenever the editable buffer, trimmed, exactly equals `kBlankWorkOrderSkeleton`.

#### Task 1.1: Blank-skeleton comparison gating Submit

**File:** `src/woz_gui.cpp`
**Tag:** T3 — mechanical, spec-locked comparison logic, but it changes a control path on a working, in-use file, so every line gets reviewed rather than delegated outright (same reasoning as `gm-multiturn-loop.md` Task 1.2).

**Actual:** T2 — author wrote the implementation directly, with AI answering questions and reviewing the result rather than drafting it. The review caught two build-breaking issues: a stray `#include <mftransform.h>` pulling in `windows.h`'s `min`/`max` macros (broke `gm_gauntlet.hpp`'s unrelated `std::min` call), and a `string_view` constructor mismatch in the new `trim()` helper caused by checked/debug iterators on this toolchain's MSVC STL. Both were fixed by the author.

**Changes:**
- Add a small helper (or inline expression) computing `is_blank = trim(wo_text) == trim(kBlankWorkOrderSkeleton)`, evaluated once per frame at the point `wo_text` is already resynced from `wo_text_buf` (`woz_gui.cpp:296`, existing sync point).
- Wrap the existing "Submit Turn" `ImGui::Button` call in `ImGui::BeginDisabled(is_blank || session_over)` / `EndDisabled()` — `session_over`'s existing gate (`woz_gui.cpp:298`) is preserved, `is_blank` is additive.

**Acceptance Criteria:**
- On launch (blank buffer), "Submit Turn" renders disabled and a click has no effect.
- Typing any one differing character re-enables it immediately.
- Clicking "Load into editor" on any saved/session entry re-enables it immediately (unless the loaded content happens to equal the blank skeleton exactly).
- After a successful Submit resets the buffer to blank, the button goes disabled again on the next frame.

**Rollback:** git revert the phase's commit(s).

---

### Phase 2: Draft Management — Reset & Save — Done

**Objective:** A GM can discard an in-progress edit back to blank in one click (Reset), or promote the current buffer to a reusable `workorders/*.json` template (Save), without leaving the app.

#### Task 2.1: Reset button

**File:** `src/woz_gui.cpp` (extends Task 1.1)
**Tag:** T3 — reads the exact same `is_blank` boolean Task 1.1 already computes, no new comparison logic or decision, but it authors a new button and click handler on the same working control-path area as Task 1.1, so it gets the same "every line reviewed" treatment.

**Changes:**
- Add an `ImGui::SameLine()` "Reset" button beside "Submit Turn", disabled under the same `is_blank` condition (not `session_over` — spec §4 locks Reset as ungated by session-over).
- On click: `wo_text = kBlankWorkOrderSkeleton; snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());` — same reset pattern Submit's own success path already uses (`woz_gui.cpp:315-316`).

**Acceptance Criteria:**
- On launch, "Reset" renders disabled.
- After typing or loading any differing content, "Reset" is clickable.
- Clicking it restores the buffer to exactly `kBlankWorkOrderSkeleton`, with no confirmation step.
- "Reset" stays enabled/clickable even after the session-over banner appears (only "Submit Turn" freezes).

#### Task 2.2: Save button, filename field, and validation

**File:** `src/woz_gui.cpp` (extends Task 2.1)
**Tag:** T2 — real validation logic (allowlist check, collision check, trailing-`.json` strip) with correctness stakes (a bad filename must never reach the actual file write); worth an AI-drafted pass with author Q&A rather than blind delegation.

**Actual:** T2 — AI drafted the full implementation (helper functions plus the Save click handler) as a walkthrough; author typed it in and iterated through several AI-reviewed rounds before landing. Review caught three issues along the way: an inverted `BeginDisabled(can_save)` that disabled Save exactly when the name was valid; an early extension-stripping helper that truncated at the first (then any) dot rather than only a trailing `.json`, silently mangling names; and a `parse_error` write that ran unconditionally every frame instead of only inside a click handler, clobbering Submit's own error message the same frame it was set. A follow-up round also caught `is_valid_filename` and `strip_json_suffix` disagreeing on what counts as an extension (arbitrary `.ext` names silently passed validation, then got double-extensioned on write) and a validate-before-strip ordering that rejected legitimate `foo.json` input; both fixed by reordering to strip-once-then-validate. Author verified all Task 2.2 acceptance criteria pass after the final fix.

**Changes:**
- Add a per-frame `std::string save_filename` and a fixed-capacity `char save_filename_buf[256]` alongside the existing `wo_text`/`wo_text_buf` pair (`woz_gui.cpp:154-155` pattern) — 256 comfortably covers any reasonable template name plus the appended `.json`, consistent with the rest of the panel using plain `snprintf`-sized buffers rather than dynamic strings for widget-backing storage.
- Add an inline `InputText` for the filename, `SameLine()` with "Save", disabled while the field is empty (mirrors the existing "Load into editor" disabled-while-nothing-selected pattern at `woz_gui.cpp:279-280`).
- Validate the filename with a plain character scan (`std::all_of` over `isalnum(c) || c == '_' || c == '-'`), not `<regex>` — nothing else in this codebase includes `<regex>`, and a single fixed-charset check doesn't need it.
- On click, in order — cheapest/most-obviously-wrong check first: (1) validate `save_filename` fully matches the allowlist above — refuse with an inline error if not; (2) strip a trailing `.json` if present, then append `.json`; (3) call the existing `parse_work_order(wo_text)` — refuse with the same inline parse-error styling `parse_error` already renders (`woz_gui.cpp:327-329`) if it throws; (4) check `fs::exists("workorders/" + name)` — refuse with an inline error on collision; (5) write the buffer to `workorders/<name>.json` and push `<name>.json` onto `saved_workorder_files` in memory. This ordering isn't spec-locked (spec §4 only fixes allowlist-before-collision); filename-shape-before-parse is this plan's own call, since it's the cheapest check and the spec's own precedent (Submit's handler) already treats parse failures as the expensive/rare path.
- All four failure modes (empty field — button disabled, not a runtime check; allowlist; parse; collision) share one inline-error display, reusing the existing `parse_error`-style rendering rather than a second error-message mechanism.

**Acceptance Criteria:**
- Saving a valid, uniquely-named buffer creates `workorders/<name>.json` on disk and the name appears in the Saved group immediately, without restarting `woz_gui`.
- Saving a malformed (non-parsing) buffer shows an inline error and writes nothing.
- Saving to a filename that already exists under `workorders/` shows an inline error and writes nothing.
- Saving a filename containing anything outside `[A-Za-z0-9_-]` (e.g. `foo/bar`, `foo:bar`, `..`) shows the same inline error and writes nothing.
- Typing `foo.json` in the filename field (extension included) writes `workorders/foo.json`, not `workorders/foo.json.json`.
- Save stays enabled/clickable after the session-over banner appears (only "Submit Turn" freezes).

**Rollback:** git revert the phase's commit(s).

---

### Phase 3: Resizable, Persisted Splitters — Done

**Objective:** All three named fixed-size sub-splits get a draggable splitter bar with a shared minimum size; ratios persist across relaunches via `gui_layout.json`, with back-computed defaults on first launch or on any load/parse failure. Only Task 3.4 below depends on Phase 2; Tasks 3.1–3.3 and 3.5 don't touch the Reset/Save row and have no such dependency.

Given this phase is the hardest in the plan to verify by code inspection alone (drag interactions, a persistence round-trip, a resize-vs-drag clamp subtlety — see Task 3.2), consider a `docs/exec/`-style walkthrough companion doc before implementing, mirroring the precedent `gm-multiturn-loop.md`'s own Phase 3 set (`docs/exec/phase-3-woz-gui.md`) for exactly this kind of manual-verification-heavy GUI work. Not required — the acceptance criteria below are sufficient on their own — but it's this repo's established pattern for phases like this one.

#### Task 3.1: Persisted layout state

**File:** `src/woz_gui.cpp`
**Tag:** T3 — deterministic JSON load/save/fallback logic with a direct precedent already in the file (`saved_workorder_files`'s startup try/catch scan, `woz_gui.cpp:157-168`); mechanical once that pattern is followed, but touches a working file.

**Changes:**
- A small struct, e.g. `struct LayoutRatios { float sidebar = 220.0f/640.0f; float preview = 120.0f/400.0f; float log_list = 150.0f/400.0f; };` — defaults computed once from the locked `640×400` reference (spec §4): `sidebar ≈ 0.34375`, `preview = 0.30`, `log_list = 0.375`.
- `LayoutRatios load_layout_or_default()`: attempts to read + parse `gui_layout.json` (`nlohmann::json`, same convention as every other JSON fixture in the repo); on missing file, unreadable file, or parse failure, returns a default-constructed `LayoutRatios` — one fallback path, no branching between "missing" and "corrupt" (spec §4), matching the same try/catch-and-fall-back shape the existing `workorders/` startup scan already uses.
- `void save_layout(const LayoutRatios&)`: writes `gui_layout.json`. Not called from within this task — Tasks 3.3/3.4's splitter call sites are what actually trigger a write, on drag-release (see Task 3.2).
- `load_layout_or_default()` runs once at startup (mirroring the existing `saved_workorder_files` startup-scan pattern at `woz_gui.cpp:157-168`), held as per-frame-loop state alongside `wo_text`/`saved_workorder_files`/etc.

**Acceptance Criteria:**
- `load_layout_or_default()` returns the back-computed defaults when `gui_layout.json` doesn't exist.
- `load_layout_or_default()` returns the same defaults (not a crash, not an exception escaping) when given a hand-corrupted/truncated `gui_layout.json`.
- `save_layout()` followed immediately by `load_layout_or_default()` round-trips the exact ratios written.

#### Task 3.2: Interactive drag-splitter widget

**File:** `src/woz_gui.cpp` (uses Task 3.1's `LayoutRatios`)
**Tag:** T2 — the one piece of this plan with a real, non-obvious correctness property (the resize-vs-drag clamp reconciliation below) worth an AI-drafted pass with author review, not mechanical wiring.

**Changes:**
- A `bool splitter(const char* id, float& ratio, float total_extent, bool horizontal)` helper: renders an `InvisibleButton` sized to a thin strip, tracks `IsItemActive()`/`GetIO().MouseDelta` to update `ratio` in place while dragging, and returns `true` on exactly the frame the drag ends (`IsItemDeactivatedAfterEdit()`) — the caller (Task 3.3/3.4's call sites), not this helper, is what calls `save_layout()`, since only the caller has the full `LayoutRatios` struct in scope, not just the one field being dragged.
- The floor clamp is recomputed from `total_extent` every frame, both while idle and while dragging — `ratio` is never allowed to represent a pixel value outside `[kMinSplitPx, total_extent - kMinSplitPx]` *at the current frame's `total_extent`*. This single rule satisfies two things at once that could otherwise conflict: (a) spec §4's "don't bake in a stale clamp" intent — if the window later grows, a ratio that was floored at a small size immediately reflects its true value again, because the clamp bound is recomputed fresh each frame, not stored; (b) no drag-reversal dead zone — because the clamp applies to `ratio` itself (not just what gets rendered), overshooting the floor during a drag and then reversing direction makes the panel respond immediately, instead of requiring the mouse to first travel back through the overshot distance.

**Acceptance Criteria:**
- The clamp never lets either side compute below `kMinSplitPx` pixels, for any dragged ratio input, at a representative panel width down to ~2×`kMinSplitPx`.
- Dragging a splitter past its floor, then immediately reversing drag direction, grows the shrunk side back within a small mouse-travel distance — not only after re-covering the full overshoot (no dead zone).
- Shrinking the window (or panel) below what the current ratio would need, with no drag in progress, visually floors the render at `kMinSplitPx` without changing the stored ratio; growing the window back out restores the original proportions exactly, not a distorted one.
- `splitter()` returns `true` on exactly the frame `IsItemDeactivatedAfterEdit()` fires, and `false` on every other frame including every frame of an active drag.

#### Task 3.3: Wire the list-sidebar and log-list splitters

**File:** `src/woz_gui.cpp` (uses Task 3.2's widget; no dependency on Phase 2)
**Tag:** T3 — mechanical application of an already-built, already-tested widget to two structurally simple existing `BeginChild` pairs; touches a working file, so reviewed rather than delegated outright.

**Actual:** T2 — the log-list splitter site needed two AI review passes to converge, past what "mechanical wiring" implies. First pass found a self-referencing uninitialized-variable read (`clamp_ratio(layout.log_list, log_list_height)` reading `log_list_height` in its own initializer instead of `log_panel_height` — silent UB, since the project builds without `-Wall`/`-Wextra`) and a splitter id colliding with the adjacent detail child window's id. The author's follow-up fixed both but left the `total_extent` argument wired to `log_list_height` instead of `log_panel_height`, which would have snapped the ratio to exactly 0.5 on the very first drag at default proportions (150/400 total_extent falls under the floor-clamp's small-extent branch); fixed directly by AI. The sidebar splitter site itself had no defects of its own beyond the shared `splitter()` widget's 1px sizing bug (Task 3.2's `Actual`-worthy catch, though within that task's own T2 expectations).

**Changes:**
- Replace `##WorkOrderList`'s literal `ImVec2(220.0f, 0.0f)` (`woz_gui.cpp:251`) with a width computed from `layout.sidebar * available_width`, followed by `if (splitter("##SidebarSplit", layout.sidebar, available_width, true)) save_layout(layout);`, followed by `##WorkOrderMain`'s existing `BeginChild` (already auto-sized).
- Replace `##LogEntryList`'s literal `ImVec2(0.0f, 150.0f)` (`woz_gui.cpp:338`) with a height computed from `layout.log_list * available_height`, followed by the same splitter-then-save pattern, followed by `##LogEntryDetail`'s existing `BeginChild` (already auto-sized to `0,0`).

**Acceptance Criteria:**
- Dragging the sidebar splitter live-resizes both the list sidebar and the main WorkOrder content area within the same frame, with no scroll/redraw glitch.
- Dragging the log-list splitter live-resizes both the entry list and the detail view within the same frame.
- Closing and relaunching `woz_gui` after dragging either splitter restores the dragged-to size, not the original 220px/150px default.

#### Task 3.4: Wire the WorkOrder preview/editor splitter and update the editor's footer constant

**File:** `src/woz_gui.cpp` (uses Task 3.2's widget; **depends on Phase 2** — needs the Submit/Reset/Save/filename row to already exist)
**Tag:** T2 — the one splitter site with a real composition question already resolved at the spec level (how the ratio interacts with the editor's negative-sizing idiom) — worth a careful pass to confirm the implementation actually matches that resolution, not just that it compiles.

**Changes:**
- Replace `##WorkOrderPreview`'s literal `ImVec2(0.0f, 120.0f)` (`woz_gui.cpp:288`) with a height computed from `layout.preview * main_content_height`, followed by the splitter-then-save pattern from Task 3.3. `main_content_height` is `ImGui::GetContentRegionAvail().y` read at that exact call site — i.e., *after* the "Load into editor" button above it has already been drawn, but *before* the footer reservation below — which is what "computed against the full main-content region, footer included" (spec §4) means concretely: the footer hasn't been subtracted yet at this point in the frame, only the button row above has (by ordinary ImGui cursor advancement, not a separate calculation).
- Change the editor's `ImGui::InputTextMultiline(..., ImVec2(-1.0f, -60.0f))` (`woz_gui.cpp:294-295`) to `ImVec2(-1.0f, -N)` where `N = ImGui::GetFrameHeightWithSpacing() * 2` — one row for the Submit/Reset/Save/filename-field row (Phase 2), one row unconditionally reserved for `parse_error` text whether or not it's currently set.
- Arrange Submit, Reset, Save, and the Save filename field on one `ImGui::SameLine()`'d row beneath the editor, matching the row Task 2.1/2.2 already built — this task only changes their combined *height reservation*, not their arrangement (already correct from Phase 2).

**Acceptance Criteria:**
- Dragging the preview/editor splitter live-resizes both the preview box and the editor textbox within the same frame; the button row and any visible `parse_error` text stay pinned at a constant height below the editor, not shrinking or being pushed off-screen.
- The editor's height never visibly jumps when a parse error appears or clears (footer stays reserved either way).
- Closing and relaunching `woz_gui` after dragging this splitter restores the dragged-to size, not the original 120px default.

#### Task 3.5: `.gitignore` entry for the generated layout file

**File:** `.gitignore`
**Tag:** T4 — one-line addition, no logic.

**Changes:**
- Add `gui_layout.json` to the "Generated run/session logs" block (`.gitignore:9-12`), alongside `woz_log.jsonl`/`session_log.jsonl`/`gui_session_log.jsonl`.

**Acceptance Criteria:**
- `git status` stays clean after running `woz_gui`, dragging a splitter, and closing it — `gui_layout.json` is created on disk but never shows as untracked.

**Rollback:** git revert the phase's commit(s).

---

### Phase 4: Live Saved-Group Refresh — Done

**Objective:** The Saved group reflects `workorders/` exactly as it is on disk right now, on demand, without restarting `woz_gui`.

#### Task 4.1: Factor the startup scan and add the Refresh button

**File:** `src/woz_gui.cpp`
**Tag:** T3 — mechanical extraction of an existing loop into a function called from two sites, plus a straightforward new button handler; touches a working file, so reviewed rather than delegated outright.

**Actual:** T2 — AI drafted and applied the implementation directly rather than the author typing it in for review. One real correctness bug was caught and fixed during drafting, before it was ever shown to the author: an early version reset `wo_preview_text` whenever `selected_saved_file` ended up `< 0` after a refresh, which doesn't distinguish "nothing selected" from "a This-Session entry is selected" (the two are mutually exclusive elsewhere in the file) — as written it would have wrongly clobbered a This-Session preview on every Refresh click. Fixed by gating the whole reconciliation block on `had_saved_selection`, captured before the rescan. Author confirmed all four Acceptance Criteria pass by hand against the built binary.

**Changes:**
- Extract the existing startup `workorders/` scan (`woz_gui.cpp:157-168`, the `fs::directory_iterator` loop wrapped in try/catch) into a function `std::vector<std::string> scan_workorders_dir()`, called once at startup (replacing the inline loop) and once from the new button's click handler.
- Add a "Refresh" button beside the "Saved" `ImGui::SeparatorText` header (`woz_gui.cpp:262`), not grouped with Submit/Reset/Save.
- On click: `saved_workorder_files = scan_workorders_dir();` (full replace, not an additive merge). If `selected_saved_file` no longer indexes a valid entry in the new list (by filename, not by stale index), clear both `selected_saved_file = -1` and `wo_preview_text`. The editable `wo_text` buffer is left untouched.

**Acceptance Criteria:**
- A file copied into `workorders/` by an external process appears in the Saved group after clicking Refresh, with no restart.
- A file removed externally disappears from the Saved group after clicking Refresh.
- If the removed file was the currently-selected/previewed entry, the preview pane and selection both clear; the editable buffer is unaffected.
- If the currently-selected entry still exists after Refresh, selection and preview are unaffected.

**Rollback:** git revert the phase's commit(s).

---

## 3. Files to Modify

| Path | Purpose | Est. LOC | Risk |
|------|---------|----------|------|
| `src/woz_gui.cpp` | Submit/Reset gate, Save + filename validation, layout persistence + drag widget + 3 wiring sites, Refresh — all four phases | +250/-10 | Medium |
| `.gitignore` | Add `gui_layout.json` to the generated-files block | +1 | Low |

`gui_layout.json` itself is a runtime-generated, gitignored file (like `gui_session_log.jsonl`) — not a repo file to modify, not listed above.

---

## 4. Validation Checklist

### Functional
- [x] Submit is disabled against an untouched blank buffer and re-enables on any edit or load (Phase 1).
- [x] Reset clears the buffer to blank in one click, disabled only when there's nothing to clear (Phase 2).
- [x] Save writes a valid, uniquely, validly-named buffer to `workorders/` and it appears in the Saved group immediately (Phase 2).
- [x] Save refuses and shows an inline error, writing nothing, for: malformed JSON, filename collision, filename outside `[A-Za-z0-9_-]` (Phase 2).
- [x] All three splitters drag-resize live and persist their ratio across a close/relaunch cycle (Phase 3).
- [x] All three splitters stop at the 120px floor on both sides, with no drag-reversal dead zone after overshooting it (Phase 3).
- [x] First launch (no `gui_layout.json`) and a launch against a hand-corrupted `gui_layout.json` both render identically to today's pre-spec baseline sizes (Phase 3).
- [x] Refresh picks up externally added/removed `workorders/*.json` files with no restart, clearing selection/preview only if the selected entry was removed (Phase 4).

### Integration
- [x] Reset and Save remain usable after the session-over banner appears; only Submit freezes (Phases 1–2).
- [x] The editor's footer height never visibly jumps when `parse_error` appears/clears (Phase 3).
- [x] A floored splitter (window shrunk smaller than the stored ratio would need) restores its original proportions when the window grows back, rather than staying distorted (Phase 3).
- [x] `git status` stays clean after normal use — `gui_layout.json`/`gui_session_log.jsonl` never show as untracked (Phase 3).

### Regression
- [x] All five existing test suites (`test_gauntlet`, `test_log`, `test_applier`, `test_roll`, `test_turn`) pass unchanged — this plan touches no header they cover.
- [x] `woz` and `woz_loop` remain unaffected — this plan touches only `woz_gui.cpp`.
- [x] Submitting a valid WorkOrder still advances the turn exactly as before (Task 3.4's footer-constant change must not alter Submit's actual behavior, only its layout).

---

## 5. Risk Summary

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| The floor-clamp's "recompute every frame, don't bake in a stale bound" rule (Task 3.2) is subtle enough that a straightforward implementation could clamp only the rendered pixels and not the stored `ratio`, reintroducing a drag-reversal dead zone | Medium | Low | Task 3.2's acceptance criteria explicitly test the reversal case before any splitter is wired to a real site (Tasks 3.3/3.4) |
| Writing `gui_layout.json` on every drag-release (rather than only at clean shutdown) could fire on every intermediate frame if `IsItemDeactivatedAfterEdit()` is wired incorrectly, spamming disk I/O during a drag | Low | Low | Task 3.2's AC requires `splitter()` to return `true` on exactly the release frame; a manual drag-and-watch-file-timestamp check during Task 3.3/3.4 verification catches an over-eager write trigger |

---

## Status Legend

| Marker | Meaning |
|--------|---------|
| Not Started | Work hasn't begun |
| In Progress | Currently being implemented |
| Done | Phase implemented and verified |

Update the header `Status` field as phases complete (e.g. `In Progress (2/4 phases done)`).
