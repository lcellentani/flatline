# GM Multi-Turn Playable Loop — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/gm-adjudicator.md` |
| **Date** | 2026-07-04 |
| **Status** | In Progress (3/4 phases done) |
| **Effort** | 3–4 sessions (Phase 0 is a short but necessary bootstrap; Phases 1–2 are one light session; Phase 3's SDL3/ImGui build-out is the bulk) |

---

## 1. Overview

### Goal

After this ships, the single-room guardroom scenario (`gm-adjudicator.md` §2's first goal) is playable end-to-end as a real multi-turn session: a persistent `World` that carries forward across turns, a human GM live-authoring WorkOrders each turn, and the session ending naturally on player death or an explicit quit. This is what finally produces actual Experiment-1 "is it fun" data — nothing built so far generates it, since `wizard_of_oz_demo.cpp` still runs exactly one hardcoded turn.

### Approach

- Extract the already-proven per-turn pipeline (modifier budget → roll → gauntlet → apply → log) out of the single-turn demo into a shared `gm_turn.hpp`, so every front end calls one implementation instead of forking it — the same "one computation, not two copies" discipline already applied to `apply_one` and `modifier_magnitude_sum`.
- Two front ends share that one pipeline: a plain stdin/stdout CLI loop (`woz_loop.cpp`, file-based WorkOrder authoring, scriptable for regression runs) and an SDL3 + OpenGL3 + Dear ImGui desktop tool (`woz_gui.cpp`) for live Experiment-1 sessions, with three panels — room description, GM WorkOrder editing, and a session log (entry list + JSON detail view).
- The GM WorkOrder panel is a raw JSON text box, not a structured per-effect-type form. Matches the project's own explicit non-goal of technical polish (spec §3) and the "instance-is-schema" text-first precedent already used for every room/work-order file in the codebase; binding UI widgets to the 6-primitive `variant` is real work that tests nothing about "is it fun." A richer, form-based editor (dropdown modifier pickers, per-effect-type sub-forms) stays out of scope unless real Experiment-1 sessions show raw-JSON authoring is actually the friction point — that would justify its own SPEC, not speculative upfront investment.
- A dedicated Player Input panel was considered and cut during Phase 3 exec planning (`docs/exec/phase-3-woz-gui.md`): a free-text scratchpad with no schema field, no persistence, and no logging just gets manually retyped, in different words, into the WorkOrder's own `interpretation` field a few inches away — friction with no payoff, not a real capability. `interpretation` remains the one place player intent lives, same as it always did.
- The Log panel is a selectable entry list plus a pretty-printed JSON detail view, not a flat scrolling transcript — also decided during Phase 3 exec planning, motivated by a concrete pain point from Phase 2's own manual AC verification (a raw `session_log.jsonl` line is a single-line JSON blob, unreadable without an external pretty-printer). Reuses `gm_log.hpp`'s existing `to_json(LogEntry)` rather than inventing a second, parallel summarization.
- The GM WorkOrder panel gets the same list + detail shape, on the write side — decided in a follow-up Phase 3 exec planning pass. It keeps the raw-JSON editable buffer (previous bullet, unaffected) but adds a selectable list feeding it: this session's own past turns (already in memory) plus a `workorders/` folder of saved templates on disk. Selecting a row shows a **read-only preview**; an explicit **Load into editor** action is the only thing that overwrites the live draft, so casual browsing can never silently lose in-progress work.
- Phase 0 stands up the actual local dev environment for the first time. Everything up to now was drafted in chat and validated in an ad hoc sandbox, not on a real machine with persisted state — no repo, no git history, no toolchain. It vendors the one existing external dependency (`nlohmann/json`) and introduces Clang + Ninja + `CMakePresets.json` + clangd once, up front, covering the files that already exist — so Phases 1–3 build on a real, working local baseline instead of each assuming one already exists.
- CMake, once introduced in Phase 0, is only ever *extended*: Phase 2 adds a `woz_loop` target, Phase 3 adds `woz_gui`. Neither touches the toolchain again — the Clang/Ninja/compile_commands.json setup and the one-time GCC→Clang verification both live in Phase 0, where the switch actually first happens.

### Scope

**In Scope:**
- Local dev environment: git repo, vendored `nlohmann/json`, and the Clang + Ninja + CMake + clangd toolchain (Phase 0)
- Shared turn-processing pipeline (`gm_turn.hpp`)
- Headless CLI multi-turn loop (`woz_loop.cpp`)
- SDL3 + OpenGL3 + Dear ImGui desktop tool (`woz_gui.cpp`) with the three named panels
- Minimal CMake build covering existing + new targets, behavior-preserving for existing ones

**Out of Scope** (pulled from `gm-adjudicator.md` §3, plus additions specific to this plan):
- The LLM/Experiment-2 runtime, semantic memory, multi-room/movement, an exhaustive effect vocabulary, combat balance/progression/art (spec Non-Goals, unchanged)
- A structured/form-based WorkOrder editor — raw JSON only (decided above)
- Session-log replay/analysis tooling beyond the GUI's own live Log panel — `gm_log.hpp`'s existing `read_log`/`replay_matches` already cover offline replay
- Any visual theming beyond ImGui defaults, controller/touch input

---

## 2. Phased Task Breakdown

### Phase 0: Dev Environment & Project Setup — Done

**Objective:** Turn the file set produced during spec drafting into an actual local repo that compiles and runs the WOZ demo and all four test suites on your machine, under the same Clang + Ninja + CMake + clangd toolchain the rest of this plan builds on — everything so far has only ever been validated in an ad hoc sandbox, not a persisted local environment.

#### Task 0.1: Repo scaffold, version control, and GitHub remote — Done

**New Files:** `.gitignore`; initial `git init` + first commit; a GitHub remote
**Tag:** T4 — pure mechanical scaffolding, no design decision.

**Verified:** repo name locked as `flatline`; `origin` points at `https://github.com/lcellentani/flatline.git` (fetch+push); `main` tracks `origin/main` with no extra args needed. Initial commit `fe19dae` ("Bootstrap Flatline repo: toolchain, vendored deps, drafted GM engine") holds the full file set. Note: `.clangd` and `.zed/` are currently untracked (working-tree additions since the initial commit) — harmless, but flag if they should be committed or gitignored.

**Changes:**
- `git init` at the repo root.
- Keep the existing flat file layout (`gm_*.hpp`, `test_*.cpp`, `wizard_of_oz_demo.cpp`, `room_guardroom.json`, `workorder_*.json`) exactly as drafted — no speculative `src/` restructuring. **Superseded 2026-07-11 (Phase 3, Task 3.3 session):** JSON fixtures were later moved to a sibling `res/` directory (commit `35a29d8`) — they're data, not source, so `src/` now holds only `gm_*.hpp`/`test_*.cpp`/`wizard_of_oz_demo.cpp`/`woz_loop.cpp`/`woz_gui.cpp`/`app.ixx`/`app.cpp`/`expected.hpp`. Recorded here rather than rewritten, since this bullet captured the original Task 0.1 decision and rationale at the time it was made.
- Add a `third_party/` directory for vendored dependencies — first user is `nlohmann/json` (Task 0.2), later joined by Dear ImGui (Phase 3, Task 3.1).
- `.gitignore` excludes build output directories and generated log files (`woz_log.jsonl`, `session_log.jsonl`) — those are run artifacts, not source.
- Connect a GitHub remote by running `gh repo create` with **no name argument** — its own interactive wizard prompts for the repository name, visibility, and description, then offers to push the existing local commit as the initial state. This is deliberate, not an oversight: the project's name isn't locked yet, so the plan prompts for it at setup time instead of this document hardcoding a guess. Fallback if `gh` isn't authenticated on the machine: create an empty repo by hand on GitHub (no README/license/`.gitignore` — those would create a conflicting root commit), then `git remote add origin <url>`, `git branch -M main`, `git push -u origin main`.

**Acceptance Criteria:**
- `git log` shows one initial commit containing every file currently held as project knowledge.
- `git status` is clean after a full build — nothing generated is tracked.
- `git remote -v` shows `origin` pointing at a GitHub URL for both fetch and push.
- `git push`/`git pull` on `main` need no extra arguments afterward — upstream tracking is set.

**Note:** project codename locked as **Flatline** (rejected: Recursion, Loci, Curtain, Postern, Adjudicator, and the rest of the two brainstorming rounds). `gh repo create`'s prompt still asks at setup time — the natural answer is now `flatline`, but Ludo confirms the exact repo name when actually running Task 0.1.

#### Task 0.2: Vendor nlohmann/json — Done

**New File:** `third_party/nlohmann/json.hpp`
**Tag:** T4 — vendoring unmodified upstream source, same shape as Phase 3's ImGui vendoring.

**Verified:** `third_party/nlohmann/json.hpp` present with a `README.md` recording the pinned tag; resolves via `-I third_party` with no system install.

**Changes:**
- Vendor the single-header amalgamated release, pinned to a specific tag (recorded the same way ImGui's tag is recorded in Phase 3). The demo's own build comment currently treats this as `-I<path-to-nlohmann>` — a machine-local path only you know about; vendoring makes it a checked-in, reproducible part of the repo.

**Acceptance Criteria:**
- `#include <nlohmann/json.hpp>` resolves via the vendored path with no system-wide install needed.
- Vendored file is byte-identical to the pinned release tag (empty diff).

#### Task 0.3: CMake + Clang + Ninja + clangd, covering the existing five targets — Done

**New Files:** `CMakeLists.txt`, `CMakePresets.json`
**Tag:** T2 — matching an already-established personal toolchain pattern is mostly mechanical, but this is the very first time this code compiles under Clang anywhere outside an ad hoc sandbox, so it's a real check, not an assumption.

**Verified (2026-07-06):** `cmake --build build --preset default` builds all five targets cleanly under `clang++`/Ninja. Re-ran all five binaries from `build/bin`: `test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20 — exact match to the acceptance-criteria counts — and `woz` exits 0 with output matching the documented single-turn trace. `build/compile_commands.json` exists and `.clangd` at the repo root points `CompilationDatabase` at `build`.

**Changes:**
- `CMakePresets.json` pins the configure step: generator `Ninja`, `CMAKE_CXX_COMPILER = clang++`, `CMAKE_EXPORT_COMPILE_COMMANDS = ON` — one shared, portable config instead of per-editor settings, so VS Code's CMake Tools extension and Zed's CMake integration both pick it up with no extra setup.
- `CMakeLists.txt` defines the five targets that already exist: `test_gauntlet`, `test_log`, `test_applier`, `test_roll`, `woz` (the demo) — pointing at the vendored `nlohmann/json` from Task 0.2. This is the one CMake file Phase 2 and Phase 3 later extend with `woz_loop` and `woz_gui`; nothing here is rebuilt or migrated a second time.
- Per-editor dotfiles (`.vscode/settings.json`, Zed project settings) stay out of the repo deliberately — personal/local concern, not something this plan commits; flag it if you'd rather have those checked in too.

**Acceptance Criteria:**
- `cmake --preset <name> && cmake --build --preset <name>` builds all five targets.
- All five pass with the exact counts the last session log recorded (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, demo exit 0) — the first real proof this codebase behaves identically on your machine, under Clang, and not just in a validation sandbox.
- `compile_commands.json` is generated and clangd resolves it — verified by opening any `gm_*.hpp` in either VS Code or Zed and confirming live diagnostics, not just syntax highlighting.

**Rollback:** Phase 0 has no local "before" state to revert to — rollback is deleting the repo directory and re-extracting the drafted files.

---

### Phase 1: Shared Turn-Processing Pipeline — Done

**Objective:** Extract the per-turn pipeline already proven correct in `wizard_of_oz_demo.cpp` into a reusable function, so the CLI loop, the GUI tool, and the existing demo all call one implementation.

#### Task 1.1: New shared pipeline header — Done

**New File:** `gm_turn.hpp`
**Tag:** T2 — mechanical extraction of logic already proven in the demo, but the return shape is a small real decision worth an AI Q&A pass rather than blind delegation.

**Verified (2026-07-07):** `gm_turn.hpp` compiles as part of the `test_turn` target (18/18 new checks pass, exit 0) covering the bribe-path `Applied` outcome, the hallucinated-path `GauntletRejected` outcome (rejected at `ids_exist`), and one-log-line-per-call across all three outcome kinds — each cross-checked against an independently-run `validate_modifiers`/`resolve_turn`/`run_gauntlet`/`apply` call, not just shape-checked. `process_turn` does no console I/O (returns `std::pair<World, log::Outcome>` only), matching the plan's own reasoning for reusing `Outcome` as the report shape.

**Changes:**
- Define `std::pair<World, log::Outcome> process_turn(const World& world, int64_t turn_number, const WorkOrder& wo, const std::string& log_path)`.
- Runs, in order: modifier-budget check → roll (if `modifiers` non-empty) → gauntlet on the selected branch → apply (if the gauntlet survives) → log — exactly the sequence `wizard_of_oz_demo.cpp` runs today.
- Appends exactly one `log::LogEntry` per call, matching the already-locked one-call-per-turn invariant.
- Returns the unchanged `world` on a rejection outcome (`ModifierBudgetRejected` or `GauntletRejected`), or the newly applied `World` on `Applied`.
- Reuses `gm::log::Outcome` (`ModifierBudgetRejected` | `GauntletRejected` | `Applied`) as the report shape instead of inventing a parallel struct — every field a console or GUI renderer needs (stage, reason, narration, roll info, applied effects, vital transitions) already exists there.
- No changes to `gm_applier.hpp`, `gm_gauntlet.hpp`, `gm_roll.hpp`, `gm_log.hpp`, `gm_rules.hpp` — pure aggregation of existing calls.

**Acceptance Criteria:**
- `gm_turn.hpp` compiles standalone (`-fsyntax-only` clean).
- Calling `process_turn` with `workorder_bribe.json`'s parsed WorkOrder against `room_guardroom.json`'s parsed World returns an `Applied` outcome whose narration, applied-effect count, and resulting `World`'s gold/inventory match today's demo's printed values exactly.
- Calling it with `workorder_hallucinated.json`'s effects returns a `GauntletRejected` outcome at the same stage/effect index the current demo reports, with the returned `World` identical to the input.
- Exactly one line is appended to the given log path per call, regardless of which outcome kind is returned.

#### Task 1.2: Refactor the demo to call the shared pipeline — Done

**File:** `wizard_of_oz_demo.cpp`
**Tag:** T3 — purely mechanical replacement of inlined steps with one `process_turn` call; touches a tested, working file, so every line gets reviewed rather than delegated outright.

**Verified (2026-07-07):** Captured `woz`'s stdout + `woz_log.jsonl` before the refactor (via `git stash`), rebuilt after, diffed both — stdout byte-identical, exit code 0 both times, and the newly appended log line is structurally JSON-equal (compared via `JSON.stringify` after parse, same method `test_log.cpp` itself uses) to the pre-refactor entry. All 4 pre-existing suites re-ran with unchanged counts: `test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, all exit 0. The one incidental cleanup: `wizard_of_oz_demo.cpp`'s now-unused `#include "gm_applier.hpp"` was dropped (its only remaining need, `applier::apply`/`vital_transitions`, moved into `gm_turn.hpp`).

**Changes:**
- Replace the inlined modifier-budget → roll → gauntlet → apply → log block with a single call to `gm_turn::process_turn`.
- Steps 1–3 (load room, print state, parse work order) and 7–9 (round-trip proof, vocabulary gate, hallucination worked example) stay exactly as-is — they exercise the bridge/vocabulary directly, not the pipeline.

**Acceptance Criteria:**
- `test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20 all still pass unchanged (confirms no shared-header churn broke anything, even though this file isn't part of any test binary).
- Demo's console output is line-for-line identical to the pre-refactor run (diff of captured stdout).
- Demo exit code stays 0.
- The newly appended `woz_log.jsonl` entry is structurally identical (same fields, same values) to a pre-refactor entry.

**Rollback:** git revert the phase's commit(s).

---

### Phase 2: Headless CLI Multi-Turn Loop — Done

**Objective:** A real, scriptable multi-turn session: persistent `World` across turns, engine-tracked turn numbers, live file-based WorkOrder authoring, ending on player death or an explicit quit.

#### Task 2.1: New CLI loop — Done

**New File:** `woz_loop.cpp`
**Tag:** T2 — the file-based-authoring/quit-sentinel control flow follows directly from what was discussed; the body is mechanical reuse of `process_turn`, but a fresh entry point still warrants a quick pass.

**Verified (2026-07-11):** Exec plan `docs/exec/phase-2-woz-loop.md` executed against the built `woz_loop` binary; all four ACs confirmed (see that doc's §5 for the full walkthrough and its verification note). Summary: AC #1 — one applied turn from `workorder_bribe.json`, byte-for-byte matching `woz`'s own trace (die 1, FAILURE, same narration/gold/inventory), logged as `turn_number == 1`. AC #2 — a continuous two-turn session (bribe fails, then a follow-up key request succeeds) produces two log entries where `entry[1].world_before` is structurally identical to `entry[0].world_after`. AC #3 — the new `src/workorder_lethal-smoke.json` fixture drives `player.hp` 20→7→(raw)−6 across two turns, crossing `kDeathOvershoot`; `=== SESSION OVER: player is dead ===` prints immediately with no further turn prompt, exit 0, and the final log entry's `vital_transitions` shows `alive → dead` (overkill −6). AC #4 — with no `workorder_turn.json` staged, typing `quit` at turn 1 exits 0 immediately with no parse error and no `session_log.jsonl` created, proving the quit sentinel short-circuits before the file read. All five pre-existing suites re-ran at unchanged counts (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18, `woz` exit 0).

**Changes:**
- Load a room once from `argv[1]` (default `room_guardroom.json`) into a persistent `World world`.
- Loop, with an engine-tracked `int64_t turn_number` starting at 1:
  - Print `dump(world)` and the turn number.
  - Print instructions: edit `workorder_turn.json` with the ruling, save it, then press Enter — or type `quit` to end the session.
  - Read one line from stdin; if it case-insensitively equals `quit`, print a session summary and break *without* reading `workorder_turn.json` for that iteration.
  - Otherwise `slurp("workorder_turn.json")` → `parse_work_order` → `gm_turn::process_turn(world, turn_number, wo, "session_log.jsonl")`; print the returned outcome (narration, roll, or rejection reason); reassign `world` to the returned `World`; `++turn_number`.
  - After an `Applied` outcome, check `rules::session_over(world)`; if true, print a session-over message and break.
- Uses a session-specific log file (`session_log.jsonl`), distinct from the demo's `woz_log.jsonl`, so real session data never interleaves with demo/test output.
- Its plain stdin/stdout shape (no GUI event loop) is what makes it convenient to drive from a script for regression runs — a wrapper script can pre-stage a sequence of `workorder_turn.json` files and pipe blank lines to advance turns automatically.
- Adds a `woz_loop` target to Phase 0's `CMakeLists.txt` — same header-only dependencies as the demo, no new libraries, no toolchain changes.

**Acceptance Criteria:**
- Running `./woz_loop` with `workorder_bribe.json`'s contents staged as `workorder_turn.json`, then pressing Enter once, produces one applied turn identical in effect to the current demo's single turn, logged with `turn_number == 1`.
- Editing `workorder_turn.json` between two Enter-presses produces two log entries with `turn_number == 1` and `2`, where turn 2's `world_before` equals turn 1's `world_after` — the concrete proof that turn 2 actually reads real updated state, which nothing built so far demonstrates.
- A staged sequence of WorkOrders that drives the player's hp past `kDeathOvershoot` ends the loop after the applying turn, printing the session-over message, without a further prompt.
- Typing `quit` ends the loop immediately without attempting to read `workorder_turn.json` for that iteration.

**Rollback:** git revert the phase's commit(s) — new file only, zero risk to existing files.

---

### Phase 3: SDL3 + OpenGL3 + Dear ImGui Desktop Tool — In Progress

**Objective:** A live, three-panel Experiment-1 session tool built on the same shared pipeline, for the actual "is it fun" playtests.

#### Task 3.1: Vendor SDL3 + Dear ImGui + OpenGL3 backends — Done

**New Files:** `third_party/SDL3/**` (full upstream source tree); `third_party/imgui/**` (core: `imgui.h/.cpp`, `imconfig.h`, `imgui_internal.h`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imstb_*.h`; backends: `imgui_impl_sdl3.cpp/.h`, `imgui_impl_opengl3.cpp/.h/_loader.h`)
**Tag:** T4 — vendoring unmodified upstream source; no authored logic, nothing to own.

**Verified (2026-07-11):** Both vendored per `docs/exec/phase-3-woz-gui.md` §1 (that doc records the reasoning for the two real decisions made here — SDL3 vendored as full source rather than vcpkg/system-install, and ImGui pinned to the `docking` branch rather than a tagged `master` release). SDL3 at tag `release-3.4.12` (commit `f87239e`), full tree, `.git` stripped, vendoring note at `third_party/SDL3/VENDOR.md` (not `README.md` — SDL3 ships its own, which stays untouched). ImGui at `docking`-branch commit `6029ee3`, selective file list, vendoring note at `third_party/imgui/VENDOR.md`. One real gap surfaced during Task 3.2's build and was fixed: `imconfig.h` is required by `imgui.h` but was missing from the original vendor list; fetched at the pinned commit and added, and the exec plan's own file list corrected to match.

**Changes:**
- Vendor SDL3's full upstream source tree, pinned to a specific release tag, as a plain source drop (no `.git`, not a submodule).
- Vendor a pinned Dear ImGui `docking`-branch commit with its official SDL3 + OpenGL3 backend pair — selective file list, not the whole upstream tree.
- Record the exact vendored tag/commit for both in a short vendoring note so each is reproducible later.

**Acceptance Criteria:**
- Vendored files are byte-identical to the pinned upstream tag/commit (empty diff).
- The vendored tag/commit is recorded somewhere in the repo, for both SDL3 and ImGui.

#### Task 3.2: Add the `woz_gui` target — Done

**File:** `CMakeLists.txt` (extends Phase 0's file)
**Tag:** T2 — wiring `find_package(SDL3)`/OpenGL correctly is a real "does this link on your machine" risk worth a quick pass; the toolchain itself (Clang, Ninja, presets, compile_commands) is already in place from Phase 0 and isn't touched here.

**Verified (2026-07-11):** `cmake --preset default && cmake --build build --preset default` succeeds end to end — SDL3 built as a static lib via `add_subdirectory` (not `find_package`, since it's vendored as source per Task 3.1, not installed), `imgui` built as a static lib linking `SDL3::SDL3` + `OpenGL::GL`, and `woz_gui` links against both. `woz_gui.exe` runs and exits 0. A minimal placeholder `src/woz_gui.cpp` was added to actually exercise this AC ahead of Task 3.3 — not the real shell, just enough (`SDL_Init`/`Quit`, `ImGui::CreateContext`/`DestroyContext`) to prove the link is real. All five pre-existing suites re-ran at unchanged counts (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18), plus `woz` exit 0 and `woz_loop` untouched.

**Changes:**
- Vendor options set before `add_subdirectory(third_party/SDL3)`: static link only (`SDL_STATIC ON`/`SDL_SHARED OFF`), no test library — avoids a runtime DLL to copy alongside `woz_gui.exe`.
- `find_package(OpenGL REQUIRED)` — system dependency, ships with the OS/SDK, not vendored.
- A hand-rolled `imgui` static library target (ImGui has no CMake build of its own) compiling the Task 3.1 file list, linking `SDL3::SDL3` + `OpenGL::GL` `PUBLIC` so `woz_gui` picks up both transitively.
- Add a `woz_gui` target linking the `imgui` library.
- No changes to `CMakePresets.json` or to the five pre-existing targets — they're already on Clang/Ninja since Phase 0.

**Acceptance Criteria:**
- `cmake --build` (existing preset) additionally produces `woz_gui`.
- `woz_gui` links successfully against SDL3 and OpenGL.
- The five pre-existing targets are unaffected — same pass counts as Phase 0 left them.

#### Task 3.3: SDL3/OpenGL3/ImGui application shell — Done

**New File:** `woz_gui.cpp` (shell only — window, context, main loop)
**Tag:** T4 — boilerplate lifted almost verbatim from Dear ImGui's own official SDL3+OpenGL3 example; no project-specific decision in window/context setup.
**Actual:** T3 — the shell itself matched plan (boilerplate lifted from ImGui's example), but implementation also introduced the project's first C++20 module (`engine.app`) and a project-wide exceptions ban with a new `Expected<T,E>` type, neither scoped by this task. See note below and the gaps flagged back to Ludo.

**Verified (2026-07-12):** `cmake --build build --preset default` succeeds clean; `woz_gui.exe` builds and links. All five pre-existing suites re-ran at unchanged counts (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18); `woz` exits 0. Shipped shape differs from this task's own "New File"/"Changes" text above: the shell is not a single free-function `woz_gui.cpp` but a C++20 module split (`src/app.ixx` + `src/app.cpp`, module `engine.app`, exporting a factory-constructed `App` class) with `woz_gui.cpp` reduced to a thin `main()`. That split, plus a project-wide exceptions ban (`src/expected.hpp`, engine-wide migration) and a `src/` → `res/` fixture move, are real deviations from both this doc and the Phase 3 exec plan — flagged to Ludo rather than silently reconciled here, since they touch Files to Modify, Task 0.1's locked layout text, and Task 3.4's exec-plan draft, none of which this update is scoped to rewrite.

**Changes:**
- Window creation, ImGui context init/shutdown, event pump, frame render/present — no game logic yet.

**Acceptance Criteria:**
- Running `woz_gui` opens a window and renders an empty ImGui frame without crashing, closing cleanly on window-close.
- No ImGui/SDL3 console warnings (unmatched `Begin`/`End`, double context init, etc.).

#### Task 3.4: Three-panel layout and turn submission

**File:** `woz_gui.cpp` (extends Task 3.3's shell)
**Tag:** T2 — panel layout and interaction flow (what each panel owns, when Submit is enabled) are real UI decisions worth a quick pass; wiring into `process_turn`/`dump`/`log::Outcome` is mechanical reuse of already-proven code.

**Changes:**
- **Room Description panel** — read-only scrollable text rendering `dump(world)`, the same function already used by the demo and the CLI loop.
- **GM WorkOrder panel** — a multi-line raw-JSON text box (`InputTextMultiline`) as the editable submit buffer, alongside a list sidebar with two groups: this session's own past turns, and saved files from a `workorders/` folder. Selecting a list row shows a read-only preview; a "Load into editor" button copies it into the editable buffer. A "Submit Turn" button parses the editable buffer's contents via the existing, unchanged `parse_work_order` and calls `gm_turn::process_turn`.
- **Log panel** — a selectable list of one-line entries (turn number, status, short teaser) plus a detail sub-pane showing the selected entry's full `log::LogEntry`, pretty-printed as JSON (reuses `gm_log.hpp`'s existing `to_json`, so the view is structurally identical to the corresponding line in the session log file). Newly-submitted entries auto-select themselves.
- Malformed JSON in the WorkOrder panel is caught at the Submit handler and shown inline as a parse error — it does not advance `turn_number`, touch `World`, or add an entry to either the Log panel or the WorkOrder panel's session-history group (new behavior the CLI/demo never needed, since they only ever read well-formed files off disk).
- On `rules::session_over(world)` after an applied turn: a session-over banner is shown and Submit is disabled; the window stays open so the log/history remains viewable.
- Session log written to its own file, mirroring `woz_loop`'s convention.

**Acceptance Criteria:**
- Room Description panel's text matches `dump(world)` byte-for-byte for the same `World`.
- Submitting `workorder_bribe.json`'s contents pasted into the panel advances the turn exactly as `woz_loop` would for the same input — same narration, same resulting gold/inventory, same log-entry shape.
- Submitting malformed JSON shows an inline error and leaves `turn_number` and `World` unchanged.
- The Log panel's entry list accumulates one row per submitted turn in submission order; selecting a row shows its full `LogEntry` as pretty-printed JSON, structurally identical to the corresponding line in the session log file.
- Selecting a row in the WorkOrder panel's list (either group) updates a read-only preview without altering the editable buffer; only the explicit "Load into editor" action overwrites it.
- A staged sequence of WorkOrders driving the player to `Dead` shows the session-over banner and disables Submit, without closing the window.

**Rollback:** git revert the phase's commit(s) — this phase only adds `woz_gui.cpp`, the vendored ImGui sources, and one new CMake target; Phase 0's toolchain and the five pre-existing targets are untouched either way.

---

## 3. Files to Modify

| Path | Purpose | Est. LOC | Risk |
|------|---------|----------|------|
| `.gitignore` | **New** (Phase 0) — excludes build output and generated log files | ~10 | Low |
| `third_party/nlohmann/json.hpp` | **New** (Phase 0) — vendored, unmodified upstream | N/A (vendored) | Low |
| `CMakeLists.txt` | **New** (Phase 0) — 5 existing targets; extended in Phase 2 (`woz_loop`) and Phase 3 (`woz_gui`) | ~70 total | Medium |
| `CMakePresets.json` | **New** (Phase 0) — pins Ninja + `clang++` + compile-commands export for clangd | ~20 | Low |
| `gm_turn.hpp` | **New** — shared per-turn pipeline (modifier budget → roll → gauntlet → apply → log) | ~70 | Low |
| `wizard_of_oz_demo.cpp` | Refactor steps 4–6c to call `gm_turn::process_turn`; steps 1–3, 7–9 untouched | -60/+15 | Low |
| `woz_loop.cpp` | **New** — headless multi-turn CLI loop, file-based authoring, scriptable for regression runs | ~90 | Medium |
| `third_party/imgui/**` | **New** — vendored Dear ImGui + SDL3 backend + OpenGL3 backend, unmodified upstream | N/A (vendored) | Low |
| `woz_gui.cpp` | **New** — thin `main()`: `import engine.app;`, `App::create()`/`.run()`. Panels (list+detail Log) + turn submission (Task 3.4) render inside `App::run`'s frame callback | ~15 | Medium |
| `src/app.ixx` | **New** (Phase 3, Task 3.3) — `engine.app` module interface: `App` class owning the SDL3/OpenGL3/ImGui window+context lifecycle and main loop, factory-constructed via `Expected<App, std::string>` | ~55 | Low |
| `src/app.cpp` | **New** (Phase 3, Task 3.3) — `engine.app` module implementation (window/context init, event pump, frame render/present, shutdown) | ~135 | Low |
| `src/expected.hpp` | **New** (Phase 3, Task 3.3) — dependency-free `Expected<T,E>`; introduced for `App::create()` and then adopted project-wide as the return-value alternative once exceptions were banned (see `.claude/rules/coding_style.md`) | ~60 | Low |
| `gm_json.hpp`, `gm_log.hpp` | Migrated off exceptions to return `Expected<T,E>` (`parse_work_order`/`parse_world`/`read_log`); call sites updated in `gm_turn.hpp`, `wizard_of_oz_demo.cpp`, `woz_loop.cpp`, and all four `test_*.cpp` suites | ~110 total | Low |
| `res/*.json` | **Moved** from `src/` (Phase 3, Task 3.3 session, commit `35a29d8`) — room/WorkOrder fixtures are data, not source; only `CMakeLists.txt`'s fixture paths changed | N/A (moved) | Low |
| `.claude/rules/coding_style.md` | Documents the exceptions ban, `Expected<T,E>` convention, the "constructors that can fail" static-factory pattern, and module file-naming (`snake_case.ixx`/`.cpp`) | ~45 | Low |

---

## 4. Validation Checklist

### Functional
- [x] A 3+ turn session via `woz_loop` threads world state correctly turn to turn — confirmed 2026-07-11 (Task 2.1, AC #2/#3).
- [ ] A 3+ turn session via `woz_gui`, given the same WorkOrders, produces mechanically identical outcomes to `woz_loop`.
- [ ] Quit sentinel (CLI) / window close (GUI) ends the session cleanly at any point.
- [ ] Session ends automatically, on both front ends, when `player.vital` reaches `Dead`.

### Integration
- [x] `wizard_of_oz_demo.cpp`'s output is byte-identical before/after the Phase 1 refactor — confirmed 2026-07-07.
- [ ] Clang/Ninja-built test binaries produce identical pass counts to the current manual `g++` builds.
- [ ] clangd gives live diagnostics on `gm_*.hpp` in both VS Code and Zed once `compile_commands.json` exists.
- [ ] Malformed WorkOrder JSON in the GUI panel fails gracefully — no crash, no `World` mutation.

### Regression
- [x] All four existing test suites unmodified and still passing (22/22, 37/37, 31/31, 20/20) — confirmed 2026-07-06 under Phase 0's Clang/Ninja build; re-confirmed 2026-07-07 after the Phase 1 refactor, plus new `test_turn` 18/18.
- [x] Demo still exits 0 — confirmed 2026-07-06.

---

## 5. Risk Summary

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| SDL3 isn't installed/discoverable on the build machine — first external binary dependency this project has had | Medium | Medium | Confirm `find_package(SDL3 REQUIRED)` resolves before writing any app code (Task 3.2 is sequenced ahead of 3.3/3.4 for exactly this reason) |
| Raw-JSON WorkOrder box has no schema validation before Submit; a bad paste throws from `parse_work_order` | Medium | Low | Catch at the Submit handler, show the parse error inline, don't advance the turn (Task 3.4) |
| The very first Clang compile of this codebase (Phase 0) could surface latent GCC-specific behavior (extensions, differently-worded warnings) the code happens to rely on, even though nothing in it looks GCC-specific | Low | Medium | Task 0.3's acceptance criteria requires identical pass counts under Clang, not just a successful compile, before Phase 0 counts as done |

---

## Status Legend

| Marker | Meaning |
|--------|---------|
| Not Started | Work hasn't begun |
| In Progress | Currently being implemented |
| Done | Phase implemented and verified |

Update the header `Status` field as phases complete (e.g. `In Progress (2/4 phases done)`).
