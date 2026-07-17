# Exec Plan: Phase 3 — Resizable, Persisted Splitters (`woz_gui.cpp`)

**Source plan:** `docs/plans/woz-gui-qol.md`, Phase 3, Tasks 3.1–3.5 (currently "Not Started").

## Context

Phases 1–2 are Done: "Submit Turn" is gated against a blank buffer, and Reset/Save let a GM discard or promote a draft. Phase 3 replaces the three hardcoded `BeginChild` sizes in `src/woz_gui.cpp` (list-sidebar `220×0`, WorkOrder preview `0×120`, Log entry-list `0×150`) with draggable, persisted splitters, per `docs/specs/woz-gui-qol.md` §4. All line numbers below are verified against the current 463-line `src/woz_gui.cpp`.

One correctness property drives most of the design (spec §4, plan Task 3.2, plan Risk Summary): the 120px floor clamp must be recomputed every frame from the panel's *current* size, but it must **not** silently overwrite the persisted ratio just because the window happens to be small this frame — only an actual drag may change the persisted value. Conflating the two would either let a one-off small window permanently distort a dragged-to ratio, or reintroduce a drag-reversal dead zone. The design below keeps them as two values: `layout.sidebar`/`preview`/`log_list` (persisted, mutated only by an active drag) and a per-frame `clamp_ratio(...)` result (used only to size this frame's regions).

---

## 1. New constants and helpers (Tasks 3.1–3.2)

Inserted as a new block after `is_valid_filename()` (`woz_gui.cpp:90-98`) and before the `// --- main app` marker (`woz_gui.cpp:100`) — grouped together since `clamp_ratio`/`splitter` both depend on `kMinSplitPx`, and `LayoutRatios`/`load_layout_or_default`/`save_layout` are this same block's data-side counterpart.

```cpp
constexpr float kMinSplitPx = 120.0f;
constexpr float kSplitterThickness = 4.0f;
constexpr const char* kLayoutPath = "gui_layout.json";

// Back-computed from today's hardcoded pixel baselines (220/120/150) against a
// rounded 640x400 reference -- half the SDL window's default 1280x800
// (woz_gui.cpp:138-139), matching the WorkOrder/Log panels' actual 0.5/0.5
// DockBuilder split (woz_gui.cpp:266,269). Reused as-is on first launch (no
// gui_layout.json yet) and as the fallback whenever the file is missing,
// unreadable, or fails to parse -- one set of defaults, not two.
struct LayoutRatios {
    float sidebar = 220.0f / 640.0f;
    float preview = 120.0f / 400.0f;
    float log_list = 150.0f / 400.0f;
};

static LayoutRatios load_layout_or_default() {
    try {
        const std::string raw = slurp(kLayoutPath);
        if (raw.empty()) {
            return LayoutRatios{};
        }
        const nlohmann::json j = nlohmann::json::parse(raw);
        LayoutRatios layout;
        layout.sidebar = j.at("sidebar").get<float>();
        layout.preview = j.at("preview").get<float>();
        layout.log_list = j.at("log_list").get<float>();
        return layout;
    } catch (const std::exception&) {
        return LayoutRatios{};  // missing, unreadable, or corrupt -- one fallback path
    }
}

static void save_layout(const LayoutRatios& layout) {
    nlohmann::json j;
    j["sidebar"] = layout.sidebar;
    j["preview"] = layout.preview;
    j["log_list"] = layout.log_list;
    std::ofstream(kLayoutPath) << j.dump(2);
}

// Floor-clamps `ratio` against `total_extent` for *this frame's* rendering
// only -- never mutates the caller's stored ratio. Recomputed fresh every
// frame so a ratio squeezed by a small window immediately reflects its true
// value again once the window grows back (spec §4: "don't clamp the stored
// fraction").
static float clamp_ratio(float ratio, float total_extent) {
    const float min_ratio = total_extent > 2.0f * kMinSplitPx ? kMinSplitPx / total_extent : 0.5f;
    return std::clamp(ratio, min_ratio, 1.0f - min_ratio);
}

// Renders a thin drag bar between two regions. While the user is actively
// dragging, updates `ratio` in place (via clamp_ratio, so it can never
// represent less than the floor at this frame's total_extent) -- this is what
// avoids a drag-reversal dead zone: the persisted value tracks the clamped
// value live during interaction, so reversing direction after an overshoot
// moves it immediately instead of needing to re-travel the overshoot first.
// Idle frames (no active drag) never touch `ratio`, only the caller's own
// clamp_ratio() call (used to size the region drawn just before this widget).
// Returns true on exactly the frame the drag ends.
static bool splitter(const char* id, float& ratio, float total_extent, bool horizontal) {
    ImVec2 size = horizontal ? ImVec2(kSplitterThickness, -1.0f) : ImVec2(-1.0f, kSplitterThickness);
    ImGui::InvisibleButton(id, size);

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) {
        ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    }

    const ImGuiCol color = active ? ImGuiCol_SeparatorActive : (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(color));

    if (active) {
        const float delta = horizontal ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
        if (delta != 0.0f) {
            ratio = clamp_ratio(ratio + delta / total_extent, total_extent);
            // InvisibleButton doesn't call this internally the way Slider/Drag
            // widgets do -- without it, IsItemDeactivatedAfterEdit() below never
            // fires (ImGui only treats an item as "edited" if something marks
            // it). imgui_internal.h is already vendored and included here for
            // the DockBuilder API (woz_gui.cpp:6); confirm MarkItemEdited's
            // signature against the pinned commit at implementation time.
            ImGui::MarkItemEdited(ImGui::GetItemID());
        }
    }

    return ImGui::IsItemDeactivatedAfterEdit();
}
```

**Why `IsItemDeactivatedAfterEdit()` and not the simpler `IsItemDeactivated()`:** the plan's own Task 3.2 AC names `IsItemDeactivatedAfterEdit()` specifically, and it's the better choice here regardless — `IsItemDeactivated()` would fire (and trigger a `save_layout()` disk write) on a plain click-with-no-movement on the splitter bar, which the plan's Risk Summary explicitly flags as worth avoiding ("spamming disk I/O"). `AfterEdit` only fires when the drag actually changed `ratio`.

**Acceptance Criteria (Task 3.1):**
- `load_layout_or_default()` returns the back-computed defaults when `gui_layout.json` doesn't exist.
- `load_layout_or_default()` returns the same defaults (no crash, no escaping exception) given a hand-corrupted/truncated `gui_layout.json`.
- `save_layout()` immediately followed by `load_layout_or_default()` round-trips the exact ratios written.

**Acceptance Criteria (Task 3.2):**
- The clamp never lets either side compute below `kMinSplitPx` pixels, for any dragged ratio, down to a panel width of ~2×`kMinSplitPx`.
- Dragging a splitter past its floor, then immediately reversing, grows the shrunk side back within a small mouse-travel distance — not only after re-covering the full overshoot.
- Shrinking the window/panel below what the current ratio needs, with no drag in progress, visually floors the render at `kMinSplitPx` **without changing the stored ratio**; growing the window back out restores the original proportions exactly.
- `splitter()` returns `true` on exactly the frame `IsItemDeactivatedAfterEdit()` fires, `false` on every other frame including every frame of an active drag.

---

## 2. State: load layout at startup (part of Task 3.1)

Added to the per-frame-loop state block, alongside `saved_workorder_files` (`woz_gui.cpp:188`), loaded immediately before the `workorders/` startup scan (`woz_gui.cpp:203-214`):

```cpp
LayoutRatios layout = load_layout_or_default();
```

---

## 3. Wire the sidebar splitter — WorkOrder List vs. Main (Task 3.3)

`woz_gui.cpp:292-317`. Horizontal split (side-by-side regions, vertical drag bar).

```diff
 ImGui::Begin(kWorkOrderTitle);

-ImGui::BeginChild("##WorkOrderList", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders);
+const float wo_panel_width = ImGui::GetContentRegionAvail().x;
+const float sidebar_width = clamp_ratio(layout.sidebar, wo_panel_width) * wo_panel_width;
+
+ImGui::BeginChild("##WorkOrderList", ImVec2(sidebar_width, 0.0f), ImGuiChildFlags_Borders);
 ImGui::SeparatorText("This Session");
 ... (unchanged: This Session loop, Saved loop)
 ImGui::EndChild();

 ImGui::SameLine();

+if (splitter("##SidebarSplit", layout.sidebar, wo_panel_width, /*horizontal=*/true)) {
+    save_layout(layout);
+}
+
+ImGui::SameLine();
+
 ImGui::BeginChild("##WorkOrderMain");
```

`wo_panel_width` is captured once, right after `Begin(kWorkOrderTitle)`, before anything else is drawn in the panel this frame — it's the full width available to split between sidebar and main content.

**Acceptance Criteria:** dragging live-resizes both the list sidebar and main content area within the same frame, no scroll/redraw glitch; closing and relaunching restores the dragged-to size, not the 220px default.

---

## 4. Wire the log-list splitter — Log entry-list vs. detail (Task 3.3)

`woz_gui.cpp:420-431`. Vertical split (stacked regions, horizontal drag bar).

```diff
 ImGui::Begin(kLogTitle);

-ImGui::BeginChild("##LogEntryList", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders);
+const float log_panel_height = ImGui::GetContentRegionAvail().y;
+const float log_list_height = clamp_ratio(layout.log_list, log_panel_height) * log_panel_height;
+
+ImGui::BeginChild("##LogEntryList", ImVec2(0.0f, log_list_height), ImGuiChildFlags_Borders);
 ... (unchanged: entry list loop)
 ImGui::EndChild();

+if (splitter("##LogListSplit", layout.log_list, log_panel_height, /*horizontal=*/false)) {
+    save_layout(layout);
+}
+
 ImGui::BeginChild("##LogEntryDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
```

**Acceptance Criteria:** dragging live-resizes both entry list and detail view within the same frame; persists across relaunch (not the 150px default).

---

## 5. Wire the preview/editor splitter + footer constant (Task 3.4)

`woz_gui.cpp:317-334`. **Depends on Phase 2** (Reset/Save row must already exist — it does, Phase 2 is Done). Vertical split.

```diff
 ImGui::BeginChild("##WorkOrderMain");

 bool has_preview = (selected_session_turn >= 0) || (selected_saved_file >= 0);
 ImGui::BeginDisabled(!has_preview);
 if (ImGui::Button("Load into editor")) {
     ... (unchanged)
 }
 ImGui::EndDisabled();

-ImGui::BeginChild("##WorkOrderPreview", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders);
+const float main_content_height = ImGui::GetContentRegionAvail().y;
+const float preview_height = clamp_ratio(layout.preview, main_content_height) * main_content_height;
+
+ImGui::BeginChild("##WorkOrderPreview", ImVec2(0.0f, preview_height), ImGuiChildFlags_Borders);
 ImGui::TextUnformatted(wo_preview_text.c_str());
 ImGui::EndChild();

+if (splitter("##PreviewSplit", layout.preview, main_content_height, /*horizontal=*/false)) {
+    save_layout(layout);
+}
+
 ImGui::Separator();

-ImGui::InputTextMultiline("##WorkOrderEditor", wo_text_buf, sizeof(wo_text_buf), ImVec2(-1.0f, -60.0f));
+const float footer_height = ImGui::GetFrameHeightWithSpacing() * 2.0f;
+ImGui::InputTextMultiline("##WorkOrderEditor", wo_text_buf, sizeof(wo_text_buf), ImVec2(-1.0f, -footer_height));
 wo_text = wo_text_buf;

 ... (unchanged: is_blank, Submit/Reset/Save/filename row, parse_error render)

 ImGui::EndChild();
```

`main_content_height` is captured right after the "Load into editor" button (matching spec §4: "computed against the full main-content region, footer included" — the footer hasn't been subtracted yet at this point, only the button row above has, via ordinary cursor advancement). `footer_height` reserves two rows: one for the Submit/Reset/Save/filename row, one *unconditionally* for `parse_error` text whether or not it's currently set — this is why the editor's height must never visibly jump when an error appears or clears. The editor keeps its existing negative-sizing idiom (`ImVec2(-1.0f, -footer_height)`); ImGui consumes whatever remains after the ratio-sized preview box and splitter bar, so the footer reservation composes with no extra math (no explicit `content_height - footer_height` calculation needed).

**Acceptance Criteria:** dragging live-resizes both preview and editor; button row and `parse_error` text stay pinned at constant height (no jump on error appear/clear); persists across relaunch (not the 120px default).

---

## 6. `.gitignore` addition (Task 3.5)

```diff
 # Generated run/session logs (run artifacts, not source)
 woz_log.jsonl
 session_log.jsonl
 gui_session_log.jsonl
+gui_layout.json
```

**Acceptance Criteria:** `git status` stays clean after running `woz_gui`, dragging a splitter, and closing it.

---

## Files touched

| Path | Purpose |
|---|---|
| `src/woz_gui.cpp` | New `LayoutRatios`/`load_layout_or_default`/`save_layout`/`clamp_ratio`/`splitter` block; three `BeginChild` sites switched from literal pixel sizes to persisted-ratio sizes plus a splitter call; editor footer constant changed from `-60.0f` to `-GetFrameHeightWithSpacing()*2` |
| `.gitignore` | Adds `gui_layout.json` |

`gui_layout.json` itself is a runtime-generated, gitignored file (like `imgui.ini`/`gui_session_log.jsonl`) — not a repo path.

No `CMakeLists.txt` changes: `nlohmann::json`, `<filesystem>`, `<algorithm>`, and `imgui_internal.h` are all already included/vendored/linked for this file (spec §"How It Fits Existing Systems").

---

## Manual verification walkthrough (run from `build/bin`)

**Task 3.1 — persisted layout state:**
1. Delete any `gui_layout.json`. Launch `woz_gui`. Confirm sidebar/preview/log-list render at today's baseline proportions.
2. Close, hand-corrupt `gui_layout.json` (truncate mid-object), relaunch. Confirm no crash, same baseline proportions as step 1.

**Task 3.2 — drag mechanics (verified once wired at §3–5 below):**
3. Drag any splitter slowly toward one side until it visibly stops. Confirm neither side ever reaches zero/negative size.
4. At the floor, reverse drag direction slightly. Confirm the shrunk side grows back immediately, not only after re-traveling the full overshoot.
5. With no drag in progress, shrink the OS window until a splitter's stored ratio can't satisfy both floors. Confirm the panel visually floors at 120px. Grow the window back to its original size. Confirm the split returns to its *exact* pre-shrink proportions, not a distorted one.

**Task 3.3 — sidebar and log-list splitters:**
6. Drag the WorkOrder panel's sidebar/main splitter and the Log panel's list/detail splitter. Confirm both live-resize within the same frame, no glitch.
7. Close and relaunch `woz_gui`. Confirm both dragged-to sizes persisted.

**Task 3.4 — preview/editor splitter:**
8. Drag the WorkOrder panel's preview/editor splitter. Confirm both resize live.
9. Trigger a `parse_error` (submit malformed JSON) and clear it (submit something valid). Confirm the editor's height never visibly jumps in either direction.
10. Close and relaunch. Confirm this split also persisted.

**Task 3.5 — `.gitignore`:**
11. `git status` after all of the above. Confirm `gui_layout.json` exists on disk but never shows as untracked.

**Regression:**
12. Full rebuild (`cmake --build build --preset default`). Confirm unchanged pass counts: `test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18, `woz` exit 0, `woz_loop` unaffected. Confirm Phase 1/2 behavior (Submit gate, Reset, Save) is unaffected — this phase only changes sizing/layout, not those handlers' logic.

---

## Verification (after implementation)

1. `cmake --build build --preset default` — builds cleanly, unchanged pass counts on all five suites plus `woz`/`woz_loop`.
2. Run the walkthrough above against the built `woz_gui` binary; record results here the same way the sibling exec docs do.
