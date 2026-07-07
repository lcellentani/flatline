# GM Multi-Turn Playable Loop — Implementation Plan

| Field | Value |
|-------|-------|
| **Spec** | `docs/specs/gm-adjudicator.md` |
| **Date** | 2026-07-04 |
| **Status** | In Progress (1/4 phases done) |
| **Effort** | 3–4 sessions (Phase 0 is a short but necessary bootstrap; Phases 1–2 are one light session; Phase 3's SDL3/ImGui build-out is the bulk) |

---

## 1. Overview

### Goal

After this ships, the single-room guardroom scenario (`gm-adjudicator.md` §2's first goal) is playable end-to-end as a real multi-turn session: a persistent `World` that carries forward across turns, a human GM live-authoring WorkOrders each turn, and the session ending naturally on player death or an explicit quit. This is what finally produces actual Experiment-1 "is it fun" data — nothing built so far generates it, since `wizard_of_oz_demo.cpp` still runs exactly one hardcoded turn.

### Approach

- Extract the already-proven per-turn pipeline (modifier budget → roll → gauntlet → apply → log) out of the single-turn demo into a shared `gm_turn.hpp`, so every front end calls one implementation instead of forking it — the same "one computation, not two copies" discipline already applied to `apply_one` and `modifier_magnitude_sum`.
- Two front ends share that one pipeline: a plain stdin/stdout CLI loop (`woz_loop.cpp`, file-based WorkOrder authoring, scriptable for regression runs) and an SDL3 + OpenGL3 + Dear ImGui desktop tool (`woz_gui.cpp`) for live Experiment-1 sessions, with four panels — player input, GM WorkOrder editing, room description, session log.
- The GM WorkOrder panel is a raw JSON text box, not a structured per-effect-type form. Matches the project's own explicit non-goal of technical polish (spec §3) and the "instance-is-schema" text-first precedent already used for every room/work-order file in the codebase; binding UI widgets to the 6-primitive `variant` is real work that tests nothing about "is it fun."
- The Player Input panel is a free-text scratchpad only — it doesn't get its own schema field or persistence. The information it captures already has a home: the WorkOrder's own `interpretation` field, which the GM types based on it and which is already logged. No parallel storage of the same fact.
- Phase 0 stands up the actual local dev environment for the first time. Everything up to now was drafted in chat and validated in an ad hoc sandbox, not on a real machine with persisted state — no repo, no git history, no toolchain. It vendors the one existing external dependency (`nlohmann/json`) and introduces Clang + Ninja + `CMakePresets.json` + clangd once, up front, covering the files that already exist — so Phases 1–3 build on a real, working local baseline instead of each assuming one already exists.
- CMake, once introduced in Phase 0, is only ever *extended*: Phase 2 adds a `woz_loop` target, Phase 3 adds `woz_gui`. Neither touches the toolchain again — the Clang/Ninja/compile_commands.json setup and the one-time GCC→Clang verification both live in Phase 0, where the switch actually first happens.

### Scope

**In Scope:**
- Local dev environment: git repo, vendored `nlohmann/json`, and the Clang + Ninja + CMake + clangd toolchain (Phase 0)
- Shared turn-processing pipeline (`gm_turn.hpp`)
- Headless CLI multi-turn loop (`woz_loop.cpp`)
- SDL3 + OpenGL3 + Dear ImGui desktop tool (`woz_gui.cpp`) with the four named panels
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
- Keep the existing flat file layout (`gm_*.hpp`, `test_*.cpp`, `wizard_of_oz_demo.cpp`, `room_guardroom.json`, `workorder_*.json`) exactly as drafted — no speculative `src/` restructuring.
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

### Phase 1: Shared Turn-Processing Pipeline — Not Started

**Objective:** Extract the per-turn pipeline already proven correct in `wizard_of_oz_demo.cpp` into a reusable function, so the CLI loop, the GUI tool, and the existing demo all call one implementation.

#### Task 1.1: New shared pipeline header

**New File:** `gm_turn.hpp`
**Tag:** T2 — mechanical extraction of logic already proven in the demo, but the return shape is a small real decision worth an AI Q&A pass rather than blind delegation.

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

#### Task 1.2: Refactor the demo to call the shared pipeline

**File:** `wizard_of_oz_demo.cpp`
**Tag:** T3 — purely mechanical replacement of inlined steps with one `process_turn` call; touches a tested, working file, so every line gets reviewed rather than delegated outright.

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

### Phase 2: Headless CLI Multi-Turn Loop — Not Started

**Objective:** A real, scriptable multi-turn session: persistent `World` across turns, engine-tracked turn numbers, live file-based WorkOrder authoring, ending on player death or an explicit quit.

#### Task 2.1: New CLI loop

**New File:** `woz_loop.cpp`
**Tag:** T2 — the file-based-authoring/quit-sentinel control flow follows directly from what was discussed; the body is mechanical reuse of `process_turn`, but a fresh entry point still warrants a quick pass.

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

### Phase 3: SDL3 + OpenGL3 + Dear ImGui Desktop Tool — Not Started

**Objective:** A live, four-panel Experiment-1 session tool built on the same shared pipeline, for the actual "is it fun" playtests.

#### Task 3.1: Vendor Dear ImGui + SDL3 + OpenGL3 backends

**New Files:** `third_party/imgui/**` (core: `imgui.cpp/.h`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`; backends: `imgui_impl_sdl3.cpp/.h`, `imgui_impl_opengl3.cpp/.h`)
**Tag:** T4 — vendoring unmodified upstream source; no authored logic, nothing to own.

**Changes:**
- Vendor a pinned Dear ImGui release tag with its official SDL3 + OpenGL3 backend pair.
- Record the exact vendored tag/commit in a comment or short README so it's reproducible later.

**Acceptance Criteria:**
- Vendored files are byte-identical to the pinned upstream tag (empty diff).
- The vendored tag/commit is recorded somewhere in the repo.

#### Task 3.2: Add the `woz_gui` target

**File:** `CMakeLists.txt` (extends Phase 0's file)
**Tag:** T2 — wiring `find_package(SDL3)`/OpenGL correctly is a real "does this link on your machine" risk worth a quick pass; the toolchain itself (Clang, Ninja, presets, compile_commands) is already in place from Phase 0 and isn't touched here.

**Changes:**
- Add a `woz_gui` target linking the vendored ImGui/backends (Task 3.1) against `find_package(SDL3 REQUIRED)` and OpenGL.
- No changes to `CMakePresets.json` or to the five pre-existing targets — they're already on Clang/Ninja since Phase 0.

**Acceptance Criteria:**
- `cmake --build` (existing preset) additionally produces `woz_gui`.
- `woz_gui` links successfully against SDL3 and OpenGL.
- The five pre-existing targets are unaffected — same pass counts as Phase 0 left them.

#### Task 3.3: SDL3/OpenGL3/ImGui application shell

**New File:** `woz_gui.cpp` (shell only — window, context, main loop)
**Tag:** T4 — boilerplate lifted almost verbatim from Dear ImGui's own official SDL3+OpenGL3 example; no project-specific decision in window/context setup.

**Changes:**
- Window creation, ImGui context init/shutdown, event pump, frame render/present — no game logic yet.

**Acceptance Criteria:**
- Running `woz_gui` opens a window and renders an empty ImGui frame without crashing, closing cleanly on window-close.
- No ImGui/SDL3 console warnings (unmatched `Begin`/`End`, double context init, etc.).

#### Task 3.4: Four-panel layout and turn submission

**File:** `woz_gui.cpp` (extends Task 3.3's shell)
**Tag:** T2 — panel layout and interaction flow (what each panel owns, when Submit is enabled) are real UI decisions worth a quick pass; wiring into `process_turn`/`dump`/`log::Outcome` is mechanical reuse of already-proven code.

**Changes:**
- **Room Description panel** — read-only scrollable text rendering `dump(world)`, the same function already used by the demo and the CLI loop.
- **Player Input panel** — a free-text field, display-only, no schema field of its own (see Approach).
- **GM WorkOrder panel** — a multi-line raw-JSON text box (`InputTextMultiline`), pre-seeded with a blank WorkOrder skeleton each turn. A "Submit Turn" button parses its contents via the existing, unchanged `parse_work_order` and calls `gm_turn::process_turn`.
- **Log panel** — scrollable, append-only view built from each turn's returned `log::Outcome`: player's stated intent, GM's narration, roll info, or rejection reason, in submission order.
- Malformed JSON in the WorkOrder panel is caught at the Submit handler and shown inline as a parse error — it does not advance `turn_number` or touch `World` (new behavior the CLI/demo never needed, since they only ever read well-formed files off disk).
- On `rules::session_over(world)` after an applied turn: a session-over banner is shown and Submit is disabled; the window stays open so the log/history remains viewable.
- Session log written to its own file, mirroring `woz_loop`'s convention.

**Acceptance Criteria:**
- Room Description panel's text matches `dump(world)` byte-for-byte for the same `World`.
- Submitting `workorder_bribe.json`'s contents pasted into the panel advances the turn exactly as `woz_loop` would for the same input — same narration, same resulting gold/inventory, same log-entry shape.
- Submitting malformed JSON shows an inline error and leaves `turn_number` and `World` unchanged.
- The Log panel accumulates one entry per submitted turn in submission order, each showing the player's stated intent alongside the GM's narration/outcome.
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
| `woz_gui.cpp` | **New** — SDL3 + OpenGL3 + Dear ImGui desktop tool: 4 panels + turn submission | ~300 | Medium |

---

## 4. Validation Checklist

### Functional
- [ ] A 3+ turn session via `woz_loop` threads world state correctly turn to turn.
- [ ] A 3+ turn session via `woz_gui`, given the same WorkOrders, produces mechanically identical outcomes to `woz_loop`.
- [ ] Quit sentinel (CLI) / window close (GUI) ends the session cleanly at any point.
- [ ] Session ends automatically, on both front ends, when `player.vital` reaches `Dead`.

### Integration
- [ ] `wizard_of_oz_demo.cpp`'s output is byte-identical before/after the Phase 1 refactor.
- [ ] Clang/Ninja-built test binaries produce identical pass counts to the current manual `g++` builds.
- [ ] clangd gives live diagnostics on `gm_*.hpp` in both VS Code and Zed once `compile_commands.json` exists.
- [ ] Malformed WorkOrder JSON in the GUI panel fails gracefully — no crash, no `World` mutation.

### Regression
- [x] All four existing test suites unmodified and still passing (22/22, 37/37, 31/31, 20/20) — confirmed 2026-07-06 under Phase 0's Clang/Ninja build.
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
