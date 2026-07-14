# woz_gui QoL Improvements — SPEC

| Field | Value |
|-------|-------|
| **Status** | Draft |
| **Date** | 2026-07-14 |
| **Plan** | Not yet planned |
| **Supersedes** | N/A |

---

## 1. Problem

Four friction points surfaced from actually running Experiment-1 sessions in `woz_gui` (`docs/plans/gm-multiturn-loop.md` Phase 3, Task 3.4):

1. **Submit is always clickable, even against an untouched blank WorkOrder.** The GM WorkOrder panel's editable buffer starts pre-seeded with `kBlankWorkOrderSkeleton` (all-empty fields, `"ruling": "allow"`). Nothing stops a click on "Submit Turn" while that buffer is still exactly the blank skeleton — advancing `turn_number`, calling `process_turn`, and adding a spurious log entry for a ruling nobody actually authored.
2. **The WorkOrder and Log panels' internal sub-sections have fixed pixel sizes, and scrolling alone doesn't fix it.** `src/woz_gui.cpp`'s Task 3.4 implementation hardcodes three `BeginChild` sizes: the WorkOrder panel's list-sidebar (`220x0`), its preview box (`0x120`), and the Log panel's entry list (`0x150`). Each has a scrollbar, but a real session's content — a growing "This Session" turn history, a long pretty-printed `LogEntry`, a multi-line `narration` — regularly outgrows a fixed 120–150px box while adjacent free space in the same panel sits unused. Scroll bars mask the constraint; they don't remove it.
3. **The editor buffer has no way to abandon or keep a draft short of Submit.** Once a GM starts typing (or loads a saved/session WorkOrder and edits it), the only ways the buffer changes again are a successful Submit (which resets it to blank) or manually retyping over it by hand. There's no quick way to discard an in-progress edit back to blank, and no way to turn a good ad-hoc draft into a reusable `workorders/*.json` template without leaving the app to hand-copy a file.
4. **The Saved group only ever reflects `workorders/` as it looked at startup.** `saved_workorder_files` is scanned once when `woz_gui` launches (Task 3.4); a file added, removed, or edited on disk afterward — including via this spec's own new Save button, for entries created *outside* the running session, e.g. by another tool or a manual copy — has no way to show up in the Saved group without restarting the app.

## 2. Goals

- The "Submit Turn" button is disabled whenever the WorkOrder panel's editable buffer, trimmed of leading/trailing whitespace, is exactly equal to `kBlankWorkOrderSkeleton` — verifiable by: on launch (blank buffer), the button renders disabled and a click has no effect; typing any one differing character, or clicking "Load into editor" on any saved/session entry, re-enables it immediately.
- All three named fixed-size sub-splits — WorkOrder list-sidebar vs. main content, WorkOrder preview-box vs. editor-textbox, Log entry-list vs. detail-view — render a draggable splitter bar between their two regions, and dragging it live-resizes both sides within the current session.
- Split ratios survive closing and relaunching `woz_gui` — the sizes you last dragged to are what you get back, not the original hardcoded defaults.
- Each splitter enforces a minimum size on both sides, so a sub-panel can never be dragged down to zero/invisible and become unrecoverable without deleting persisted state.
- A "Reset" button, beside "Submit Turn", is disabled whenever the editable buffer already equals `kBlankWorkOrderSkeleton` and, when enabled, clicking it clears the buffer back to blank in one click with no confirmation step — verifiable by: on launch the button renders disabled; after typing or loading any differing content it's clickable, and clicking it restores the buffer to exactly the blank skeleton.
- A "Save" button writes the current editable buffer to `workorders/<name>.json` (from an adjacent filename field) only if it parses as a valid WorkOrder and no file of that name already exists — verifiable by: saving a valid, uniquely-named buffer creates the file on disk and the name appears in the WorkOrder panel's Saved group immediately, without restarting `woz_gui`; saving a malformed buffer or a colliding filename shows an inline error and writes nothing.
- A "Refresh" button beside the Saved group's header re-scans `workorders/` and replaces `saved_workorder_files` with exactly what's on disk right now — verifiable by: a file copied into `workorders/` by an external process/tool appears in the Saved group after clicking Refresh, with no restart; a file removed externally disappears from the list after Refresh, and if it was the currently-selected/previewed entry, the preview pane and selection both clear.

## 3. Non-Goals

- **Top-level three-panel dockspace resizing** — already handled by ImGui's own docking system (drag the panel borders between Room Description / GM WorkOrder / Log); this spec only touches sub-sections *inside* a panel, not the panels themselves.
- **A structured/form-based WorkOrder editor** — the raw-JSON `InputTextMultiline` buffer stays exactly as it is; this spec changes when Submit is clickable and how sub-regions are sized, not how a WorkOrder is authored. Matches the standing non-goal from `docs/plans/gm-multiturn-loop.md`.
- **Any change to `process_turn`/log/turn-advance semantics** — Submit's behavior once clicked is unchanged; this spec only gates *whether* it's clickable.
- **Semantic (parsed) diffing for the Submit gate** — locked below as exact trimmed text-compare against the blank skeleton; a JSON-aware "is this meaningfully different" comparison is explicitly out of scope.
- **Persisting other UI/session state** (selected log entry, in-progress `wo_text` draft, window/dock layout beyond what `imgui.ini` already covers) — out of scope; only the three split ratios get new persistence.
- **Renaming, deleting, or editing-in-place an existing `workorders/*.json` file** — Save only ever creates a new file under a name that doesn't yet exist; managing already-saved templates stays a filesystem operation outside the app, same as it is today.
- **Any confirm-modal popup pattern** — both Reset (disabled-when-nothing-to-lose) and Save (inline errors, no overwrite) are designed to need zero new confirmation dialogs; this spec deliberately doesn't introduce that UI pattern into `woz_gui`.

## 4. Design

### Key Decisions

- **Submit gate uses exact trimmed text-compare, not semantic JSON compare** — cheap (computed every frame off the same buffer already in scope, no parse attempt), and matches the buffer's existing text-first nature. Rejected: a semantic JSON-aware diff — more forgiving of incidental whitespace/field-order changes, but requires attempting a parse purely to decide button state and complicates the "buffer doesn't currently parse" edge case for no clear benefit.
- **Manual drag-splitter widget (ImGui's invisible-button + drag-delta idiom), applied at the three named sites, not a restructuring into nested dock nodes.** Keeps each of the three panels a single ImGui window with internal `BeginChild` regions — no change to Task 3.4's existing top-level `DockBuilder` bootstrap or window topology. Rejected: turning each sub-section into its own dockable child window inside a nested dockspace — would get resize, drag-to-rearrange, and free `imgui.ini` persistence, but is a materially bigger structural change than "let me resize this box" calls for.
- **A new dedicated generated file for persisted split ratios**, parallel to `imgui.ini`'s existing per-user/generated-file convention (own file, `.gitignore`d, not source) rather than piggybacking on `imgui.ini` itself. Rejected: hooking into ImGui's lower-level `ImGuiSettingsHandler` API to store custom data inside `imgui.ini` — more "native" in the sense of one settings file instead of two, but noticeably more code and unfamiliar API surface for a payload that's just 2–3 floats.
- **Each splitter enforces a fixed 120px minimum on both sides**, not a fraction-of-space minimum — a stable, predictable floor regardless of window size, generous enough to roughly match today's hardcoded baselines (preview box 120px, entry list 150px) rather than letting a dragged-down panel shrink much further than what's already considered usable.
- **Persisted split sizes are stored and applied as a fraction of available space** (e.g. `0.35`), not an absolute pixel size — keeps proportions sensible across window resizes, consistent with how ImGui's own dock-node splits already behave. The 120px floor (previous bullet) is enforced by converting the fraction against the panel's current pixel size each frame, not by clamping the stored fraction itself.
- **New file `gui_layout.json`, written on every drag-release** — matches the project's JSON-everywhere convention (`nlohmann::json` already vendored and used throughout) and the naming pattern of `gui_session_log.jsonl`. Writing on every drag-release (not only at clean shutdown) means a crash or force-quit never loses a resize, the same robustness the session log already gets from appending per-turn instead of writing once at the end.
- **The WorkOrder list sidebar's "This Session"/"Saved" sub-grouping keeps sharing one scrollable region**, unchanged from today — not raised as a problem, and splitting it further would add a fourth resizable split beyond the three this spec scopes.
- **Reset's enabled/disabled condition is the exact same "buffer differs from blank" check already computed for Submit's gate**, just read a second time by a second button — no new comparison logic, and it means Reset and Submit are never both disabled or both enabled by coincidence, only by the same underlying fact (there's an edit, or there isn't). Rejected: a confirm popup guarding the clear — redundant once the button is only clickable when something's actually at stake, and this spec deliberately avoids introducing a first confirm-modal pattern (see Non-Goals).
- **Reset sits beside "Submit Turn"** — same row, conventional Submit/Reset pairing, and both buttons now share one gating fact (previous bullet) so their enabled/disabled states visibly move together.
- **Save's filename field is a plain inline `InputText` beside the button, not a popup** — consistent with keeping this app's interaction surface flat (no modals anywhere, previous decisions and Non-Goals). Save is disabled while the field is empty, mirroring the existing "Load into editor" disabled-while-nothing-selected pattern. The typed name has `.json` appended automatically before writing, matching the `workorder_*.json` convention every other fixture in the repo already follows — the GM types a bare name, not a full filename.
- **Save validates via the existing `parse_work_order` call before writing**, reusing exactly what Submit already calls — a malformed buffer surfaces the same inline parse-error styling instead of landing a broken file on disk. On a successful write, the new filename is appended in-memory to `saved_workorder_files` so it appears in the Saved group immediately, without waiting for the next launch's startup scan.
- **A filename collision refuses the write and shows an inline error**, matching Reset's and Submit's own no-modal, no-silent-failure conventions — the GM must pick a different name; nothing already on disk is ever silently replaced.
- **Refresh lives beside the "Saved" `SeparatorText` header**, not grouped with Submit/Reset/Save — it acts on the sidebar list, not the editor buffer, so it's placed where its effect is visible rather than batched with unrelated buttons.
- **Refresh does a full re-scan that replaces `saved_workorder_files` outright**, reusing the exact same `fs::directory_iterator` scan Task 3.4 already runs once at startup, rather than an additive merge. A merge that never drops entries can't reflect external deletions, which defeats the point of a manual "show me current disk truth" action — and reusing the startup scan verbatim means there's no second, parallel "how do I read `workorders/`" implementation to maintain.
- **If the currently-selected Saved entry no longer exists after a Refresh, selection and preview both clear** (`selected_saved_file = -1`, `wo_preview_text` cleared) — consistent with the list now reflecting reality. The editable `wo_text` buffer is untouched either way, same non-destructive-browsing guarantee every other list interaction in this panel already gives.

### How It Fits Existing Systems

Touches only `src/woz_gui.cpp` (the file Task 3.4 already owns) plus a `.gitignore` entry for the new generated layout file. No change to `gm_turn.hpp`, `gm_log.hpp`, `gm_gauntlet.hpp`, `gm_applier.hpp`, `gm_rules.hpp`, or `CMakeLists.txt` — this is purely a GUI-layer change to a file that already has zero effect on the shared turn-processing pipeline or the other two front ends (`woz`, `woz_loop`).

- Submit gating reuses the existing `wo_text`/`wo_text_buf` sync already in place (`wo_text` is resynced from the raw ImGui buffer once per frame right after the widget renders) — the compare just reads `wo_text` at the same point Submit's click-handler already does.
- The three `BeginChild` calls with hardcoded `ImVec2` sizes (`##WorkOrderList`, `##WorkOrderPreview`, `##LogEntryList`) become sized from persisted, mutable ratio state instead of literal constants; a splitter widget between each pair reads/writes that same state on drag.
- Persisted layout state loads once at startup (mirroring how `saved_workorder_files` is scanned once at startup today) and is written to `gui_layout.json` on every drag-release.
- Reset and Save sit in the same button row as "Submit Turn", below the `InputTextMultiline` editor; Reset reads the same buffer-vs-blank comparison Submit's gate computes, and Save's filename `InputText` is a new small piece of per-frame state alongside the existing `wo_text`/`wo_text_buf` pair.
- Save's file write extends the same `workorders/` folder the Saved group already scans at startup (`fs::directory_iterator`, Task 3.4) — Save only ever adds to `saved_workorder_files` in memory, it doesn't re-run the startup scan.
- Refresh factors Task 3.4's startup-time `workorders/` scan into a function called from two places (startup, and this button's click handler) rather than duplicating the scan loop — same "one computation, not two copies" discipline already applied elsewhere in this codebase (e.g. `gm_turn.hpp`'s extraction from the demo, `modifier_magnitude_sum`).

## 5. Open Questions

None — every question raised during drafting (the original four, the three-decision Reset/Save follow-up, and the Refresh follow-up) is resolved and folded into §4's Key Decisions above.

---

## Status Legend

| Status | Meaning |
|--------|---------|
| Draft | Still being shaped; open questions may exist |
| Ready | Open questions resolved; safe to run `/plan-draft` |
| Implemented | All linked plan phases are Done |
| Superseded | Replaced by a newer spec — link it above |
