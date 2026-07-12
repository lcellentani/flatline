# Exec Plan: Phase 3 — SDL3 + OpenGL3 + Dear ImGui Desktop Tool (`woz_gui.cpp`)

**Source plan:** `docs/plans/gm-multiturn-loop.md`, Phase 3, Tasks 3.1–3.4 (currently "Not Started").

## Context

Phases 0–2 are Done: the toolchain (Clang + Ninja + CMake + clangd), the shared `gm_turn::process_turn` pipeline, and `woz_loop.cpp` (a scriptable stdin/stdout CLI loop) are all built and verified. Phase 3 is the actual Experiment-1 tool — a live, three-panel desktop app so a human GM can run real "is it fun" sessions instead of hand-editing a JSON file between Enter-presses.

This machine currently has **neither SDL3 nor vcpkg installed** — the plan's own Risk Summary already flagged this as the project's first external binary dependency. Several decisions were locked across two planning conversations that produced this doc, closing gaps the source plan left open:

1. **SDL3 is vendored as source** (`third_party/SDL3`, built via `add_subdirectory` under our existing Clang/Ninja toolchain) rather than pulled via vcpkg or a system install. Matches this repo's existing vendoring discipline (`third_party/nlohmann/json.hpp`, Task 0.2) and introduces no new toolchain layer — reproducible from a fresh clone with no network access at build time, at the cost of a much larger vendor drop than a single header.
2. **The panels use Dear ImGui's docking layout**, not fixed quadrants — rearrangeable/resizable, better ergonomics for a real live-play session. This requires vendoring ImGui's `docking` branch rather than a tagged `master` release, since docking has not merged into tagged releases (confirmed against upstream at planning time — see §1.2).
3. **Three panels, not four.** The source plan's Player Input panel was cut during this planning pass: it had no schema field, no persistence, and no logging by design — its entire content just got manually retyped, in different words, into the WorkOrder's own `interpretation` field a few inches away. That's friction without payoff, not a real capability, so it's gone. `interpretation` remains the one place player intent lives, same as it always did. `docs/plans/gm-multiturn-loop.md`'s Approach/Scope/Task 3.4 wording was revised to match.
4. **The Log panel is a list + detail view, not a flat scrolling transcript.** Motivated by a concrete pain point hit during Phase 2's own manual AC verification: a raw `session_log.jsonl` line is a single-line JSON blob, unreadable without piping it through an external pretty-printer. The GUI panel instead shows a selectable one-line-per-turn list, with a detail sub-pane rendering the selected entry's full JSON (`dump(2)`), reusing `gm_log.hpp`'s existing `to_json(LogEntry)` rather than inventing a second, parallel summarization.
5. **The GM WorkOrder panel gains the same list + detail shape, on the write side.** Also motivated by hands-on Phase 2 experience: the streamlined part of that workflow was drafting a WorkOrder once and reusing/adapting it, not retyping from scratch every turn. The panel keeps its raw-JSON editable buffer (§2 of the plan's own Approach already locked "raw JSON, not a structured form" — unaffected by this change) but adds a selectable list feeding it: this session's own past turns (free, already in memory, solves "continue from what I just did") plus a `workorders/` folder on disk (curated, reusable templates, solves "start from a known scenario"). Selecting a list entry shows a **read-only preview**, not a direct overwrite — an explicit **Load into editor** action is what actually replaces the live draft, so idle browsing can't silently clobber an in-progress custom WorkOrder. Still three top-level panels — this is added internal structure on the existing WorkOrder panel, mirroring the Log panel's own list+detail split (§4.4/§4.3).

Everything else below either follows mechanically from those five calls or fills in a gap the source plan left as prose ("own file," "blank WorkOrder skeleton") with a concrete, buildable shape.

**Upstream versions confirmed at planning time (2026-07-11)** — re-confirm at implementation time, since these will have moved on:
- SDL3: latest stable tag `release-3.4.12`.
- Dear ImGui `docking` branch: active, last commit `6029ee3` (2026-07-10) — the branch is alive and being kept in sync with `master`, not abandoned.

---

## 1. Vendoring (Task 3.1)

### 1.1 SDL3 — `third_party/SDL3/`

Vendor the **full upstream source tree** at tag `release-3.4.12` (or whatever's current at implementation time), not a pruned subset. This is a deliberate asymmetry with ImGui's selective vendoring (§1.2): ImGui is a fixed, short file list with no build system of its own, so pinning exactly the files we use is safe and cheap. SDL3 is a full library with its own CMake build that conditionally compiles per-subsystem internals (video backends, audio, joystick/haptics, etc.) — hand-pruning that tree is fragile and unsupported upstream. Check in the whole thing; `add_subdirectory` only builds what our CMake options ask for (§2).

- `third_party/SDL3/VENDOR.md` records the pinned tag, same pattern as Task 0.2's `nlohmann/README.md` — named `VENDOR.md` rather than `README.md` (found during implementation): SDL3's own upstream tree already ships its own `README.md`, and this is a full-tree vendor (§1.1), so `README.md` would collide with — and, if written to, corrupt — a real upstream file. `imgui`'s vendor note uses the same `VENDOR.md` name for consistency, even though it has no such collision (§1.2's vendor is selective, no upstream `README.md` included).
- Plain vendored copy (source drop, not a git submodule) — matches every other `third_party/` entry in this repo.

### 1.2 Dear ImGui (`docking` branch) — `third_party/imgui/`

Selective vendor, matching the plan's existing Task 3.1 file list, plus `imgui_internal.h` (needed for `ImGui::DockBuilder*`, used once in Task 3.4's first-run dockspace bootstrap — it's not part of the public API, but it's still a vendored upstream header, not authored code):

```
third_party/imgui/
  imgui.h / imgui.cpp
  imconfig.h                (imgui.h unconditionally #includes this — required, not optional)
  imgui_internal.h          (DockBuilder API — internal but unmodified upstream)
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  imstb_rectpack.h / imstb_textedit.h / imstb_truetype.h   (imgui.cpp's own stb deps)
  backends/
    imgui_impl_sdl3.h / imgui_impl_sdl3.cpp
    imgui_impl_opengl3.h / imgui_impl_opengl3.cpp / imgui_impl_opengl3_loader.h
```

`imgui_demo.cpp` is deliberately **not** vendored — nothing calls `ImGui::ShowDemoWindow()`, so it's dead weight. No separate GL loader library (glad/glew/gl3w): the OpenGL3 backend ships its own minimal bundled loader (`imgui_impl_opengl3_loader.h`), which is why it's listed above instead of a new `third_party/` entry.

Pinned to the `docking` branch's commit SHA (not a version tag — docking doesn't cut its own tagged releases). `third_party/imgui/VENDOR.md` records the exact SHA and the branch name, and explicitly notes *why* docking over master (§ Context above), so a future reader doesn't have to re-derive the reasoning.

**Acceptance Criteria (Task 3.1):**
- `third_party/SDL3` present, full tree, `VENDOR.md` records the pinned tag; empty diff against that tag.
- `third_party/imgui` present with exactly the file list above; `VENDOR.md` records the pinned `docking`-branch commit SHA; each vendored file is byte-identical to that commit (per-file diff, since only a subset of the upstream repo is checked in — there's no whole-tree diff to run).

**Implementation note (2026-07-11):** `imconfig.h` was missing from the original file list above (now corrected) — `imgui.h` `#include`s it unconditionally, so the build failed with `fatal error: 'imconfig.h' file not found` until it was fetched and added. Fixed by pulling it directly at the pinned commit SHA (`curl` against `raw.githubusercontent.com/ocornut/imgui/6029ee3.../imconfig.h`), same content a `git show <sha>:imconfig.h` would produce.

---

## 2. `CMakeLists.txt` changes (Task 3.2)

```diff
+# SDL3: static link, no runtime DLL to copy alongside woz_gui.exe
+set(SDL_STATIC ON CACHE BOOL "" FORCE)
+set(SDL_SHARED OFF CACHE BOOL "" FORCE)
+set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
+add_subdirectory(third_party/SDL3)
+
+find_package(OpenGL REQUIRED)
+
+add_library(imgui STATIC
+    third_party/imgui/imgui.cpp
+    third_party/imgui/imgui_draw.cpp
+    third_party/imgui/imgui_tables.cpp
+    third_party/imgui/imgui_widgets.cpp
+    third_party/imgui/backends/imgui_impl_sdl3.cpp
+    third_party/imgui/backends/imgui_impl_opengl3.cpp
+)
+target_include_directories(imgui PUBLIC
+    third_party/imgui
+    third_party/imgui/backends
+)
+target_link_libraries(imgui PUBLIC SDL3::SDL3 OpenGL::GL)
+
 flatline_add_executable(test_gauntlet src/test_gauntlet.cpp)
 flatline_add_executable(test_log src/test_log.cpp)
 flatline_add_executable(test_applier src/test_applier.cpp)
 flatline_add_executable(test_roll src/test_roll.cpp)
 flatline_add_executable(test_turn src/test_turn.cpp)
 flatline_add_executable(woz src/wizard_of_oz_demo.cpp)
 flatline_add_executable(woz_loop src/woz_loop.cpp)
+flatline_add_executable(woz_gui src/woz_gui.cpp)
+target_link_libraries(woz_gui PRIVATE imgui)
```

Confirm the exact SDL3 CMake option names (`SDL_STATIC`/`SDL_SHARED`/`SDL_TEST_LIBRARY`) against the pinned tag's own `CMakeLists.txt`/`cmake/sdlchecks.cmake` at implementation time — these are stable across recent SDL3 releases but not contractually guaranteed. No changes to `CMakePresets.json` — still Ninja + `clang++`, same as every prior phase.

**Acceptance Criteria (Task 3.2):**
- `cmake --build` (existing preset) additionally produces `woz_gui`.
- `woz_gui` links successfully against the vendored SDL3 and system OpenGL.
- The five pre-existing targets and `woz_loop` are unaffected — same pass counts as Phase 2 left them.

**Verified (2026-07-11):** `cmake --preset default && cmake --build build --preset default` succeeds end to end (SDL3 static lib, `imgui` static lib, all six pre-existing targets, and `woz_gui`). To actually exercise this AC before Task 3.3 exists, a **minimal placeholder** `src/woz_gui.cpp` was added — not Task 3.3's real windowed shell, just enough to prove the link is real rather than trivially empty: it calls `SDL_Init`/`SDL_Quit` and `ImGui::CreateContext`/`DestroyContext`, nothing else. `woz_gui.exe` runs and exits 0. All five pre-existing suites re-ran at unchanged counts (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18) plus `woz` exit 0 and `woz_loop` untouched. Task 3.3 will replace this placeholder with the real shell.

---

## 3. Application shell (Task 3.3)

`woz_gui.cpp`'s shell is boilerplate lifted almost verbatim from Dear ImGui's own official `example_sdl3_opengl3/main.cpp` — no project-specific decision here beyond picking the non-Apple GL context attributes, since this is a Windows-only build for now:

- **GL context:** core profile, version 3.0, `glsl_version = "#version 130"` — matches ImGui's own example's non-Apple branch, broadly compatible with Windows OpenGL drivers without requiring a newer core profile than the ImGui backend actually needs.
- **Init order:** `SDL_Init(SDL_INIT_VIDEO)` → set GL attributes → `SDL_CreateWindow(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)` → `SDL_GL_CreateContext` → `SDL_GL_MakeCurrent` → `SDL_GL_SetSwapInterval(1)` (vsync) → `ImGui::CreateContext()` → `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable` → **deliberately not** `ImGuiConfigFlags_ViewportsEnable` (single OS window only — multi-viewport is a separate ImGui feature we don't need; keeping it off avoids a second class of platform-specific bugs) → `ImGui_ImplSDL3_InitForOpenGL` → `ImGui_ImplOpenGL3_Init(glsl_version)`.
- **Main loop:** poll SDL events (`ImGui_ImplSDL3_ProcessEvent` + handle `SDL_EVENT_QUIT` / window-close) → `ImGui_ImplOpenGL3_NewFrame` / `ImGui_ImplSDL3_NewFrame` / `ImGui::NewFrame()` → frame body (empty in this task, filled in by Task 3.4) → `ImGui::Render()` → `glViewport` / `glClear` → `ImGui_ImplOpenGL3_RenderDrawData` → `SDL_GL_SwapWindow`.
- **Shutdown:** `ImGui_ImplOpenGL3_Shutdown` / `ImGui_ImplSDL3_Shutdown` / `ImGui::DestroyContext` / `SDL_GL_DestroyContext` / `SDL_DestroyWindow` / `SDL_Quit`.
- ImGui writes its own `imgui.ini` next to the working directory (`build/bin`) to persist window/dock layout between runs — this is a generated per-user file, not source (§6, `.gitignore`).

**Acceptance Criteria (Task 3.3):**
- Running `woz_gui` opens a window and renders an empty ImGui frame without crashing, closing cleanly on window-close.
- No ImGui/SDL3 console warnings (unmatched `Begin`/`End`, double context init, etc.).

**Verified (2026-07-11):** `src/woz_gui.cpp` replaces Task 3.2's link-proof placeholder with the real shell: init order, main loop, and shutdown order all match §3 exactly, including `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable` (no `ViewportsEnable`) and `glsl_version = "#version 130"`. The frame body between `NewFrame()`/`Render()` is empty, per this task's own AC — Task 3.4 fills it in. `cmake --build build --preset default --target woz_gui` builds clean (no warnings); running the binary produces no console output on init or shutdown. First review pass found two gaps against this plan (missing `DockingEnable`, and leftover stock ImGui demo content in the frame body instead of an empty one) — both fixed and re-verified.

---

## 4. Three-panel docked layout and turn submission (Task 3.4)

### 4.1 State (flat locals in `main()`, mirroring `woz_loop`'s style — no class needed)

```cpp
World world = parse_world(slurp(room_path));
int64_t turn_number = 1;
std::string wo_text = kBlankWorkOrderSkeleton;   // GM WorkOrder panel's editable submit buffer
std::optional<std::string> parse_error;           // set on a bad Submit, shown inline, cleared on next good Submit
std::vector<nlohmann::json> log_entries;          // one gm::log::LogEntry per process_turn call, see 4.5
int selected_entry = -1;                          // index into log_entries shown in the Log panel's detail pane

std::vector<std::string> saved_workorder_files;   // filenames under workorders/, scanned once at startup
int selected_session_turn = -1;                   // index into log_entries currently previewed (WorkOrder panel)
int selected_saved_file = -1;                     // index into saved_workorder_files currently previewed
std::string wo_preview_text;                      // whichever of the above is selected, mutually exclusive

bool session_over = false;
```

No bespoke summary struct for the Log panel — each entry is stored as the actual serialized `gm::log::LogEntry` (via `gm_log.hpp`'s existing `to_json`), the same JSON shape `process_turn` already writes to `kGuiLogPath` internally. The entry list's one-line teaser and the detail pane's pretty-print both read off that one stored value; nothing is re-derived a second way. The WorkOrder panel's preview state (`selected_session_turn`/`selected_saved_file`/`wo_preview_text`) is kept deliberately separate from `wo_text` — the editable submit buffer — so that browsing the list never touches an in-progress draft (§4.4).

### 4.2 Dockspace bootstrap

A full-viewport `DockSpace` host window is created every frame (the standard "DockSpace over full viewport" pattern). The actual **first-run split into three panels** only runs once — guarded by checking whether the dock node already has children (`ImGui::DockBuilderGetNode(dockspace_id)->IsSplitNode()`, or checking against a `static bool bootstrapped` local) — so a second run honors whatever layout the user left in `imgui.ini` instead of resetting it every launch.

Default split (easy to change later — this is a layout preference, not a load-bearing decision):
- Left column: **Room Description**, full height — the panel you're reading continuously.
- Right column, split top/bottom: **GM WorkOrder** (top) / **Log** (bottom) — the newest submission's log entry appears near where the GM just typed it.

### 4.3 The three panels

- **Room Description** — read-only, `ImGui::TextUnformatted(dump(world).c_str())` inside a scrollable child region. Same `dump()` already used by `woz` and `woz_loop`.
- **GM WorkOrder** — a narrow list sidebar (§4.4) alongside the main editable submit buffer: `ImGui::InputTextMultiline` into `wo_text` (large fixed buffer), pre-seeded with `kBlankWorkOrderSkeleton` (§4.6) each turn or replaced via the list's "Load into editor" action. A "Submit Turn" button below it, wrapped in `ImGui::BeginDisabled(session_over)` once the session is over. `parse_error`, when set, renders as red text directly under the box.
- **Log** — split into two sub-regions, not a flat scrolling transcript (revised from the original draft once Phase 2 verification made a hard-to-read raw `session_log.jsonl` line a concrete, felt problem):
  - **Entry list** — one selectable row per turn: `Turn N — <status> — <teaser>`, where `<teaser>` is read directly off the stored JSON (`narration` for `applied`, `reason` for either rejection status — both field names already exist on `gm_log.hpp`'s flattened `LogEntry` shape, no new fields needed). Clicking a row selects it; newly-appended entries auto-select themselves (`selected_entry` set to the new index right after `push_back`), so the detail pane shows what just happened with no extra click.
  - **Detail view** — the selected entry's full JSON, `nlohmann::json::dump(2)` rendered into a read-only `InputTextMultiline` (gets free scrolling and easy copy-paste for pulling one turn's log line out to share elsewhere) or a plain `TextUnformatted` over the pretty-printed string — either works, pick whichever is less code at implementation time.

### 4.4 WorkOrder list, preview, and load

The list sidebar has two groups, sharing one preview/load mechanism:

- **This Session** — one row per entry already in `log_entries` (newest first), labeled the same way as a Log panel row (`Turn N — <status> — <teaser>`). Zero setup, always available, directly solves the friction of wanting to adapt what you just submitted a turn or two ago — the exact thing that made hand-copying a file into `workorder_turn.json` feel worthwhile during Phase 2.
- **Saved** — one row per `*.json` file found under a `workorders/` folder next to the executable, scanned once at startup (`std::filesystem::directory_iterator`, skipped gracefully — empty group, no error — if the folder doesn't exist yet). Meant to hold curated, reusable templates; seed it manually by copying a few of the existing tracked `workorder_*.json` fixtures in (§7's walkthrough does this once for verification). No CMake changes and no new `.gitignore` entry needed — `workorders/` lives under `build/bin`, already covered by the top-level `build/` ignore rule, so nothing about it is tracked either way.

Selecting a row from **either** group:
```cpp
// "This Session" row i:
selected_session_turn = i; selected_saved_file = -1;
wo_preview_text = log_entries[i].at("work_order").dump(2);

// "Saved" row i:
selected_saved_file = i; selected_session_turn = -1;
wo_preview_text = slurp(("workorders/" + saved_workorder_files[i]).c_str());  // shown as-is, not re-dumped —
                                                                               // no reformatting imposed on a
                                                                               // hand-authored file (matches the
                                                                               // "instance-is-schema" text-first
                                                                               // precedent used everywhere else)
```
`wo_preview_text` renders read-only, directly under or beside the list — it never touches `wo_text`. A **"Load into editor"** button, enabled only while something is selected, is the one explicit action that copies it over:
```cpp
if (load_clicked) {
    wo_text = wo_preview_text;
    parse_error.reset();   // clear any stale error from a previous, unrelated draft
}
```
This keeps browsing non-destructive: clicking through five past turns to find the one you want never risks losing a half-written custom WorkOrder sitting in `wo_text` — only the explicit Load action does that, exactly once, on purpose. Preview/selection state is a pure browsing aid and isn't reset on Submit; it simply becomes stale (still shows whatever was last previewed) until you pick something else.

### 4.5 Submit handler

```cpp
if (submit_clicked) {
    WorkOrder wo;
    try {
        wo = parse_work_order(wo_text);
    } catch (const std::exception& ex) {
        parse_error = ex.what();   // turn_number/World untouched, wo_text left as-is to fix inline
    } else {
        auto [world_after, outcome] = turn::process_turn(world, turn_number, wo, kGuiLogPath);

        // Same LogEntry process_turn already built internally to append to
        // kGuiLogPath (gm_turn.hpp) — reconstructed here from values already
        // in scope, then serialized via gm_log.hpp's existing to_json. Not a
        // second read of the file: the GUI's copy and the on-disk line are
        // guaranteed structurally identical because both come from the same
        // to_json hook.
        log_entries.push_back(nlohmann::json(log::LogEntry{turn_number, wo, outcome}));
        selected_entry = static_cast<int>(log_entries.size()) - 1;

        world = world_after;
        ++turn_number;             // advances on every process_turn call, Applied or rejected —
        parse_error.reset();       // matches woz_loop: only a client-side parse failure holds turn_number
        wo_text = kBlankWorkOrderSkeleton;

        if (std::holds_alternative<log::Applied>(outcome) && rules::session_over(world)) {
            session_over = true;   // banner shown, Submit disabled, window stays open
        }
    }
}
```

This mirrors `woz_loop`'s own turn-advance semantics exactly: a `ModifierBudgetRejected`/`GauntletRejected` outcome still logs and still advances `turn_number` (only a malformed-JSON parse failure — the GUI's equivalent of `woz_loop`'s missing/malformed-file case — holds the turn in place).

### 4.6 Blank WorkOrder skeleton

```json
{
  "interpretation": "",
  "ruling": "allow",
  "modifiers": [],
  "effects_on_success": [],
  "narration_on_success": "",
  "effects_on_failure": [],
  "narration_on_failure": ""
}
```

Matches the rulebook's seven fields (`gm-workorder-rulebook.md` §2) in field order, so a GM editing in place sees the same shape they'd write by hand.

### 4.7 Session-over banner

Once `session_over` is true: a banner ("SESSION OVER: player is dead") rendered prominently (top of the Room Description panel is enough — no modal, no extra window), Submit disabled via `BeginDisabled`, window stays open so the Log panel's full history remains browsable. Matches `woz_loop`'s own behavior of ending the session without discarding what already happened.

**Acceptance Criteria (Task 3.4):**
- Room Description panel's text matches `dump(world)` byte-for-byte for the same `World`.
- Submitting `workorder_bribe.json`'s contents pasted into the panel advances the turn exactly as `woz_loop` would for the same input — same narration, same resulting gold/inventory, same log-entry shape.
- Submitting malformed JSON shows an inline error and leaves `turn_number` and `World` unchanged, and does not add a row to the Log panel's entry list.
- The Log panel's entry list accumulates one row per submitted turn in submission order; selecting a row (or the auto-selected newest one) shows the full `LogEntry` as pretty-printed JSON, structurally identical to the corresponding line in `gui_session_log.jsonl`.
- The WorkOrder panel's **This Session** group always has exactly one row per submitted turn so far (both groups grow/track independently of the Log panel's own list, but read the same underlying `log_entries`); the **Saved** group reflects whatever `workorders/*.json` files exist at startup.
- Selecting any WorkOrder-list row (either group) updates the read-only preview without altering `wo_text` — a custom draft in progress survives browsing untouched.
- Clicking **Load into editor** copies the current preview into `wo_text`, replacing its prior contents, and clears any stale `parse_error`.
- A staged sequence of WorkOrders driving the player to `Dead` shows the session-over banner and disables Submit, without closing the window.

---

## 5. Session log file

`gui_session_log.jsonl` — distinct from `woz_loop`'s `session_log.jsonl` and the demo's `woz_log.jsonl`, same convention (own file per front end, never interleaved). Same `process_turn(world, turn_number, wo, log_path)` call, just a different `log_path` constant.

---

## 6. `.gitignore` addition

```diff
 # Generated run/session logs (run artifacts, not source)
 woz_log.jsonl
 session_log.jsonl
+gui_session_log.jsonl
+
+# Dear ImGui's persisted window/dock layout (per-user, not source)
+imgui.ini
```

---

## 7. Manual verification walkthrough (run from `build/bin`)

Unlike `woz_loop`, this is a windowed app — these steps are meant to be run by hand, not scripted/piped.

**Task 3.1 — vendoring:**
1. `git diff` (or a fresh clone + diff against the pinned tag/commit) confirms every vendored file under `third_party/SDL3` and `third_party/imgui` is byte-identical to upstream.
2. Both `README.md` files record the pinned tag/commit and, for ImGui, the reason for pinning `docking` over `master`.

**Task 3.2 — build:**
1. `cmake --build build --preset default` — `woz_gui` builds cleanly.
2. Confirm the five pre-existing suites plus `woz_loop`'s own AC walkthrough (`docs/exec/phase-2-woz-loop.md` §5) are unaffected: `test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18, `woz` exit 0.

**Task 3.3 — shell:**
1. Run `./woz_gui`. Confirm a window opens showing an (empty, pre-3.4) ImGui frame.
2. Close the window. Confirm clean exit, no console warnings (unmatched `Begin`/`End`, double context init).

**Task 3.4 — panels and submission:**
1. Confirm three panels are visible and docked per §4.2's default split (or whatever `imgui.ini` already has from a prior run).
2. Confirm Room Description's text matches a `dump(world)` you compare against `woz_loop`'s own printed state for the same starting room.
3. Before first launch, `mkdir workorders` and copy in a couple of the existing tracked fixtures (e.g. `workorder_bribe.json`, `workorder_open-door.json`). Launch `woz_gui`, confirm both appear in the WorkOrder panel's **Saved** group; confirm the **This Session** group is empty (no turns submitted yet).
4. Select a **Saved** row. Confirm the preview pane shows that file's contents verbatim, and `wo_text` (the editable buffer) is untouched. Click **Load into editor**. Confirm `wo_text` now matches the preview.
5. Click Submit. Confirm the Log panel's entry list gains a new row (auto-selected), its detail view's `narration`/`world_after` match what `woz_loop` produces for the same input (same seeded roll — die 1, FAILURE, per Phase 2's own AC #1 run), and the WorkOrder panel's **This Session** group now has one row for the just-submitted turn.
6. Select that new **This Session** row. Confirm its preview matches the WorkOrder you just submitted. Click Load into editor, tweak one field (e.g. change a modifier's value), Submit again. Confirm the tweaked version — not the original — is what got applied.
7. Paste something syntactically invalid (e.g. delete a closing brace) into the WorkOrder panel's editable buffer, click Submit. Confirm an inline red error appears, `turn_number` doesn't advance (next successful submit still shows the expected next number), `World` is unchanged, and no new row appears in either the Log panel's entry list or the WorkOrder panel's This Session group.
8. Drive a sequence of WorkOrders (e.g. `workorder_lethal-smoke.json`'s contents, resubmitted unchanged per Phase 2's AC #3 math: 20→7→−6) until `player.hp`'s raw total crosses `kDeathOvershoot`. Confirm the session-over banner appears, Submit becomes disabled, and the window stays open with the full entry list and detail view still browsable (including both WorkOrder-panel groups).
9. Inspect `gui_session_log.jsonl`: same shape as `woz_loop`'s `session_log.jsonl` (one entry per submit, including the malformed-JSON attempt's *absence* — that one should **not** have produced a log line), and confirm each line matches the pretty-printed JSON shown in its corresponding Log-panel row (same fields, same values, just compact vs. `dump(2)`).

---

## Files touched

| Path | Purpose |
|---|---|
| `third_party/SDL3/**` | **New** — vendored SDL3 source, unmodified upstream |
| `third_party/imgui/**` | **New** — vendored Dear ImGui (`docking` branch) core + SDL3/OpenGL3 backends, unmodified upstream |
| `CMakeLists.txt` | Adds SDL3 subdirectory + options, `imgui` static library target, `woz_gui` executable target |
| `src/woz_gui.cpp` | **New** — SDL3 + OpenGL3 + Dear ImGui desktop tool: shell (3.3) + 3 docked panels (list+detail Log) + turn submission (3.4) |
| `.gitignore` | Adds `gui_session_log.jsonl`, `imgui.ini` |

`build/bin/workorders/` (the WorkOrder panel's **Saved** group source) is not a repo path — it lives inside the already-`.gitignore`d `build/` tree, seeded manually per §7's walkthrough. No tracked file, no CMake change.

---

## Verification (after implementation)

1. `cmake --build build --preset default` — `woz_gui` builds cleanly alongside the six existing targets, which must show unchanged pass counts (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18, `woz` exit 0).
2. Run the §7 walkthroughs against the built `woz_gui` binary; record results here the same way `docs/exec/phase-2-woz-loop.md` records its own post-implementation verification.

**Progress (2026-07-11):** Tasks 3.1, 3.2, and 3.3 are implemented and verified (see their own **Verified** notes above) — vendoring is in place, `CMakeLists.txt` is wired, and `src/woz_gui.cpp` now has the real SDL3+OpenGL3+ImGui shell (docking-enabled, empty per-frame body) in place of Task 3.2's link-proof placeholder. Task 3.4 (three-panel layout, WorkOrder list/preview/load, turn submission) is **not started** — §7's Task 3.4 walkthrough steps don't apply yet.
