# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

This is **Flatline**. `src/` holds `gm_*.hpp` headers (`gm_state`, `gm_rules`, `gm_roll`, `gm_gauntlet`, `gm_applier`, `gm_log`, `gm_json`, and the shared per-turn pipeline `gm_turn.hpp`), `test_*.cpp` suites, `wizard_of_oz_demo.cpp`, and JSON fixtures. The toolchain is live: CMake (`CMakeLists.txt`) + Ninja + Clang via the `default` preset in `CMakePresets.json`, building into `build/`, with `compile_commands.json` exported for clangd. Configure/build with the CMake preset (`cmake --preset default && cmake --build build`); binaries land in `build/bin/`. Vendored deps live in `third_party/` (currently nlohmann/json).

`docs/specs/gm-adjudicator.md` is `Ready` (flipped from Draft in session log 11 — every open question in its §5 is resolved or relocated). `docs/plans/gm-multiturn-loop.md` is `In Progress`: Phase 0 (toolchain) and Phase 1 (shared turn-processing pipeline, `gm_turn.hpp`) are Done; Phase 2 (headless CLI multi-turn loop) and Phase 3 (SDL3 + OpenGL3 + Dear ImGui desktop tool) are Not Started. `wizard_of_oz_demo.cpp` still only runs one hardcoded turn — the actual multi-turn playable loop (Phase 2) is the current gap, and per session log 11 §6 is what's needed to generate real Experiment 1 ("is it fun?") signal.

The project: put an LLM at the core of a roguelike as a game master that adjudicates open-ended player intent inside a fixed rules engine, rather than generating freely. Full vision, novelty argument, and platform bets: `docs/context/llm-game-master.md`.

## Document structure

- **`docs/context/llm-game-master.md`** — the durable *why* (vision, novelty, platform/model bets, methodology). Living document; specs reference it instead of re-deriving it. Read this first for any work on the project.
- **`docs/context/session-log-N-<topic>.md`** — numbered, chronological, append-only session logs. Each records: what was produced, decisions locked, gaps/open questions surfaced, and a suggested next step. **Read the highest-numbered log before starting work** — it carries the current state of open questions and what to pick up next; do not assume the spec's own text is fully caught up (session logs sometimes record a decision before it's written back into the spec).
- **`docs/specs/*.md`** — contracts (currently just `gm-adjudicator.md`). A spec owns one contract, stays `Draft` while open questions exist, and only becomes `Ready` once they're all resolved or explicitly relocated.
- **`docs/plans/*.md`** — implementation plans derived from a `Ready` spec, phased into tasks with acceptance criteria (currently just `gm-multiturn-loop.md`, status `In Progress` — see above for phase-level detail).

Status legends (defined at the bottom of each doc type): specs go `Draft → Ready → Implemented`; plans go `Not Started → In Progress → Done`. The user's own slash commands `/spec-new` (interview-first spec authoring — never fabricate goals/decisions) and `/plan-draft` (turn a `Ready` spec into a plan) drive these transitions; they are not part of this session's tool set.

Plan tasks carry a `Tag` (`T2`/`T3`/`T4`, …) signaling how much independent judgment the task needs versus mechanical execution — check a task's tag before treating it as pure delegation.

## Core contract to know before touching any spec/plan content

The adjudicator contract (`docs/specs/gm-adjudicator.md`) is the load-bearing design so far. Its shape, in one paragraph: the GM (human now, small LLM later) never mutates world state — it fills out a **work order** (interpretation, scope `ruling`, roll `modifiers`, and *both* success/failure effect branches, authored in one call) expressed only in a **closed effect vocabulary** (`damage`, `modify_disposition`, `transfer_item`, `modify_resource`, `set_object_state`, `set_flag`). A deterministic **gauntlet** validates the roll-selected branch (existence → preconditions, folded forward across the batch → magnitude bounds) and rejects atomically on first failure; the engine, not the GM, owns the dice (d20 + modifier sum vs. a fixed DC) and the replay log (one JSON-Lines entry per turn, seeded so any run replays deterministically).

Two orthogonal validation experiments gate each other: **Experiment 1** ("is it fun?" — a human plays GM behind the validator, no inference needed) must pass before **Experiment 2** ("can the target small model — Gemma 4 E4B — actually do it?" — schema-valid rate, hallucinated-id rate, etc.) is worth running. Don't conflate the two when reading or extending specs — a design flaw and a model-capability flaw look identical if this distinction is dropped.

When editing `gm-adjudicator.md` or drafting a follow-on spec, preserve its established discipline: every `(LOCKED)` decision records the alternatives rejected and why; open questions that turn out to depend on not-yet-built infrastructure get explicitly *relocated* to a future spec (via `/spec-new`) rather than answered speculatively.
