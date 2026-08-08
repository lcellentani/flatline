# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working in this repository.

## What this repository is

**Flatline** puts an LLM at the core of a roguelike as a game master that adjudicates open-ended player intent inside a fixed rules engine, rather than generating freely. Full vision, novelty argument, and platform/model bets live in `docs/context/llm-game-master.md` — read it first for any work on the project.

This is a research repo before it is a game: most of its value is in `docs/`, and the code exists to make two specific experiments runnable. Treat the documents as primary sources, not as commentary on the code.

## Repository layout

- **`src/*.hpp`** — the engine, as header-only `gm_*` units: `gm_effects` (effect vocabulary + `WorkOrder`), `gm_state` (world model), `gm_rules` (validator reference tables), `gm_gauntlet` (validation stages), `gm_roll` (seeded dice), `gm_applier` (state deltas), `gm_log` (JSON-Lines replay log), `gm_json` (serialization bridge), `gm_turn` (the shared per-turn pipeline `process_turn`). `woz_gui_detector.hpp` is front-end-specific, not engine.
- **`src/test_*.cpp`** — standalone test executables: `gauntlet`, `applier`, `roll`, `log`, `turn`, `woz_gui_detector`.
- **`src/` front ends** — `wizard_of_oz_demo.cpp` (single hardcoded turn), `woz_loop.cpp` (scriptable stdin/stdout multi-turn CLI), `woz_gui.cpp` (SDL3 + OpenGL3 + Dear ImGui desktop tool; the front end real Experiment-1 sessions are played on).
- **`res/*.json`** — room and work-order fixtures. **`third_party/`** — vendored nlohmann/json, SDL3, Dear ImGui (docking branch).

## Build & test

```
cmake --preset default      # Clang + Ninja, exports compile_commands.json for clangd
cmake --build build         # binaries land in build/bin/
```

Tests are plain executables, not ctest — run each `build/bin/test_*`; exit code 0 plus a printed pass count is green.

**Run every binary from `build/bin/`, not the repo root.** Post-build steps copy `res/*.json` next to each binary, and four of the six test suites (`gauntlet`, `applier`, `turn`, `woz_gui_detector`) load those fixtures by bare relative path; `woz_gui` additionally reads a `workorders/` folder and writes session logs relative to its CWD. Running from elsewhere fails on missing fixtures rather than on a real regression.

C++ style is defined in `.claude/rules/coding_style.md` (auto-loaded). Run `/clang-format` on touched files before committing.

## Where current state lives

**Do not trust status snapshots — including the one below.**

- Every spec, plan, and investigation doc carries its own **Status** field. That field is authoritative over any summary anywhere else.
- **Read the highest-numbered `docs/context/session-log-N-*.md` before starting work.** Session logs are numbered, chronological, and append-only; the latest carries the current open questions and suggested next step, and a log sometimes records a decision before it is written back into the spec.
- Work occasionally runs ahead of the newest session log. If the git history shows commits past the last log, read those commits too — the log is not a guaranteed high-water mark.

Durable orientation (verified 2026-08-08, re-verify against the docs):

- The adjudicator contract (`docs/specs/gm-adjudicator.md`) is **Implemented** end to end, and the Wizard-of-Oz tooling is built (`gm-multiturn-loop`, `woz-gui-qol`, `woz-gui-new-session`, `player-input-logging` all Done).
- Active build work is `woz-gui-orchestrator-flow` (spec Ready, plan In Progress) — a structured turn compositor for `woz_gui`.
- The Experiment-2 investigation program (`docs/context/investigation-plan-experiment-2.md`) is open, with no deep dive concluded yet. DD-1 is blocked on a thin Experiment-1 replay-log corpus; more played sessions are the unblocker.

## Document structure

- **`docs/context/llm-game-master.md`** — the durable *why*: vision, novelty, platform/model bets, methodology. Living document; specs reference it instead of re-deriving it.
- **`docs/context/session-log-N-<topic>.md`** — append-only session logs: what was produced, decisions locked, gaps surfaced, next step. A decision made in conversation but never written into one does not exist.
- **`docs/context/investigation-plan-<slug>.md`** — programs for problem spaces too large or uncertain for a single spec: a roster of deep dives (driving questions, artifacts, falsifiable exit criteria) whose conclusion makes downstream specs startable without fabrication.
- **`docs/specs/*.md`** — contracts. One contract per spec; `Draft → Ready → Implemented`. Stays `Draft` while open questions exist, each either resolved or explicitly *relocated* to a future spec.
- **`docs/plans/*.md`** — implementation plans derived from a `Ready` spec; `Not Started → In Progress → Done`. Tasks carry a delegation `Tag` (`T1`–`T4`) — check it before treating a task as pure delegation.
- **`docs/exec/*.md`** — per-phase execution plans: one plan phase expanded into concrete buildable steps. Written by hand (no slash command), and they sometimes revise the source plan — they say so explicitly when they do.
- **`docs/guides/*.md`** — practical references (e.g. `gm-workorder-rulebook.md`, the human GM's authoring restatement of the adjudicator contract). The spec wins if a guide disagrees with it.

## Workflow commands

Doc lifecycle transitions run through project slash commands, each backed by a skill in `.claude/skills/`. They are **interview-first** — never fabricate goals, decisions, or roster structure on the user's behalf:

- `/spec-new`, `/spec-review` — author a spec via interview; audit it before it is declared Ready.
- `/plan-draft`, `/plan-review`, `/plan-update` — derive a plan from a Ready spec; audit it; status-only updates after work sessions.
- `/investigation-new`, `/investigation-dive`, `/investigation-update` — author an Investigation Plan (hard roster-lock gate before full drafting); run one deep dive (mandatory closing ritual: locked-vs-open split, numbered session log, status update); status-only updates.
- `/git-commit` — commit using the project format `[Tn] type(scope): subject`.

## Core contract to know before touching any spec or plan

The adjudicator contract (`docs/specs/gm-adjudicator.md`) is the load-bearing design:

The GM — human now, small LLM later — **never mutates world state**. It fills out a **work order** (interpretation, scope `ruling`, roll `modifiers`, and *both* success and failure effect branches, authored in one call) expressed only in a **closed six-primitive effect vocabulary**: `damage`, `modify_disposition`, `transfer_item`, `modify_resource`, `set_object_state`, `set_flag`. A deterministic **gauntlet** validates the roll-selected branch (existence → preconditions, folded forward across the batch → magnitude bounds) and rejects atomically on first failure. The engine, not the GM, owns the dice (d20 + modifier sum vs. a fixed DC) and the replay log (one self-contained JSON-Lines entry per turn, seeded so any run replays deterministically). That vocabulary doubles as the engine's native action language: scripted actions can be precompiled work orders through the same pipeline, which makes the author — designer template, human, or model — swappable behind a single boundary.

**Two orthogonal experiments gate each other.** **Experiment 1** ("is it fun?" — a human plays GM behind the validator, no inference needed) must pass before **Experiment 2** ("can the target small model, Gemma 4 E4B, actually do it?") is worth *running*. Building E2 contracts and spikes does not break that gate; conflating the two experiments' *results* does. Never collapse this distinction when reading or extending specs — a design flaw and a model-capability flaw look identical once it is dropped.

## Working disciplines

- Every `(LOCKED)` decision records the alternatives rejected and why.
- Open questions that depend on not-yet-built infrastructure get explicitly *relocated* (to a future spec or investigation dive) rather than answered speculatively.
- Investigation-plan prior material is **leanings, not decisions** — re-examined before locking, never treated as settled, never re-derived from scratch.
- Before proposing an "obvious" change, check for a prior recorded decision it might conflict with (specs, plans, exec docs, session logs). If one exists, surface the tension honestly instead of silently overriding it.
- Don't build structure for duplication that doesn't exist yet. The repo's own kill-switch: don't build for scale before Experiment 1 proves the loop is fun.
