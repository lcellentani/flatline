# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

**Flatline** puts an LLM at the core of a roguelike as a game master that adjudicates open-ended player intent inside a fixed rules engine, rather than generating freely. Full vision, novelty argument, and platform/model bets: `docs/context/llm-game-master.md` — read it first for any work on the project.

`src/` holds the engine as `gm_*.hpp` headers (`gm_state`, `gm_rules`, `gm_roll`, `gm_gauntlet`, `gm_applier`, `gm_log`, `gm_json`, and the shared per-turn pipeline `gm_turn.hpp`), `test_*.cpp` suites, and three front ends: `wizard_of_oz_demo.cpp` (single hardcoded turn), `woz_loop.cpp` (scriptable stdin/stdout multi-turn CLI), `woz_gui.cpp` (SDL3 + OpenGL3 + Dear ImGui desktop tool for live Experiment-1 sessions). JSON fixtures live in `res/`; vendored deps in `third_party/` (nlohmann/json, SDL3, Dear ImGui docking branch).

## Build & test

```
cmake --preset default      # configure: Clang + Ninja, exports compile_commands.json for clangd
cmake --build build         # binaries land in build/bin/
```

Tests are standalone executables, not ctest: run each `build/bin/test_*` (gauntlet, applier, roll, log, turn); exit code 0 with a printed pass count is green. Post-build steps copy `res/*.json` fixtures next to every binary; `woz_gui` additionally reads a `workorders/` folder relative to its CWD, so run it from `build/bin/`. C++ style is defined in `.claude/rules/coding_style.md` (auto-loaded); run `/clang-format` on touched files before committing.

## Where current state lives

Do not trust status snapshots — including any in this file:

- Every spec, plan, and investigation doc carries its own **Status** field; that field is authoritative over any summary elsewhere.
- **Read the highest-numbered `docs/context/session-log-N-*.md` before starting work.** Session logs are numbered, chronological, and append-only; the latest one carries the current open questions and suggested next step, and logs sometimes record a decision before it's written back into the spec.

Durable orientation only (dated 2026-07-19, verify against the docs): the adjudicator contract is Implemented end to end; the WoZ tooling is built (`gm-multiturn-loop` plan Done); Experiment-1 play sessions are underway; the Experiment-2 investigation program is active (`docs/context/investigation-plan-experiment-2.md`).

## Document structure

- **`docs/context/llm-game-master.md`** — the durable *why* (vision, novelty, platform/model bets, methodology). Living document; specs reference it instead of re-deriving it.
- **`docs/context/session-log-N-<topic>.md`** — append-only session logs: what was produced, decisions locked, gaps surfaced, next step. A decision made in conversation but not written into one does not exist.
- **`docs/context/investigation-plan-<slug>.md`** — investigation programs for problem spaces too large/uncertain for a single spec: a roster of deep dives (driving questions, artifacts, falsifiable exit criteria) whose conclusion makes downstream specs startable without fabrication.
- **`docs/specs/*.md`** — contracts. One contract per spec; `Draft → Ready → Implemented`; stays `Draft` while open questions exist, each either resolved or explicitly *relocated* to a future spec.
- **`docs/plans/*.md`** — implementation plans derived from a `Ready` spec; `Not Started → In Progress → Done`. Tasks carry a delegation `Tag` (`T1`–`T4`) — check it before treating a task as pure delegation.
- **`docs/exec/*.md`** — per-phase execution plans: a plan phase expanded into concrete buildable steps, recording decisions made during exec planning (these sometimes revise the source plan — they say so explicitly when they do).
- **`docs/guides/*.md`** — practical references (e.g. `gm-workorder-rulebook.md`, the human GM's authoring restatement of the adjudicator contract; the spec is authoritative if they disagree).

## Workflow commands

All doc lifecycle transitions run through project slash commands (each backed by a skill in `.claude/skills/`); they are interview-first — never fabricate goals, decisions, or roster structure on the user's behalf:

- `/spec-new`, `/spec-review` — author a spec via interview; audit it before it's declared Ready.
- `/plan-draft`, `/plan-review`, `/plan-update` — derive a plan from a Ready spec; audit it; status-only updates after work sessions.
- `/investigation-new`, `/investigation-dive`, `/investigation-update` — author an Investigation Plan (hard roster-lock gate before full drafting); run one deep dive session (mandatory closing ritual: locked-vs-open split, numbered session log, status update); status-only updates.
- `/git-commit` — commit with the project format `[Tn] type(scope): subject`.

## Core contract to know before touching any spec/plan content

The adjudicator contract (`docs/specs/gm-adjudicator.md`) is the load-bearing design. Its shape, in one paragraph: the GM (human now, small LLM later) never mutates world state — it fills out a **work order** (interpretation, scope `ruling`, roll `modifiers`, and *both* success/failure effect branches, authored in one call) expressed only in a **closed effect vocabulary** (`damage`, `modify_disposition`, `transfer_item`, `modify_resource`, `set_object_state`, `set_flag`). A deterministic **gauntlet** validates the roll-selected branch (existence → preconditions, folded forward across the batch → magnitude bounds) and rejects atomically on first failure; the engine, not the GM, owns the dice (d20 + modifier sum vs. a fixed DC) and the replay log (one self-contained JSON-Lines entry per turn, seeded so any run replays deterministically). The vocabulary doubles as the engine's native action language: scripted actions can be precompiled work orders through the same pipeline — the author (designer template, human, model) is swappable behind one boundary.

Two orthogonal validation experiments gate each other: **Experiment 1** ("is it fun?" — a human plays GM behind the validator, no inference needed) must pass before **Experiment 2** ("can the target small model — Gemma 4 E4B — actually do it?") is worth *running* (building E2 contracts and spikes doesn't break the gate; conflating the two experiments' results does). Don't collapse this distinction when reading or extending specs — a design flaw and a model-capability flaw look identical if it's dropped.

## Working disciplines

- Every `(LOCKED)` decision records the alternatives rejected and why.
- Open questions that depend on not-yet-built infrastructure get explicitly *relocated* (to a future spec or investigation dive) rather than answered speculatively.
- Investigation-plan prior material is **leanings, not decisions** — re-examined before locking, never treated as settled, never re-derived from scratch.
- Before proposing an "obvious" change, check for a prior recorded decision it might conflict with (specs, plans, exec docs, session logs); if one exists, surface the tension honestly instead of silently overriding it.
