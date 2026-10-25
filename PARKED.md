# PARKED: Flatline

| Field | Value |
|-------|-------|
| **Status** | Parked (deliberately) |
| **Date** | 2026-10-25 |
| **Active period** | 2026-07-05 to 2026-08-09 (87 commits) |

## What it was

A research repo asking whether an LLM can act as the game master of a roguelike: it interprets open-ended player intent inside a fixed, deterministic rules engine instead of generating freely.

Two experiments, in order:
1. **Experiment 1:** is it fun? A human plays GM behind the validator, no inference needed.
2. **Experiment 2:** can a small on-device model (Gemma 4 E4B) do the same job?

## What got built

- Header-only engine (`gm_*`): closed six-primitive effect vocabulary, validator "gauntlet", seeded dice, state applier, JSON-Lines replay log.
- Three front ends, including `woz_gui` (SDL3 + OpenGL3 + Dear ImGui), the tool Experiment 1 is played on.
- The adjudicator spec is **Implemented**. Every Wizard-of-Oz tooling spec/plan pair is **Done**.

## Why I parked it

The build plan was completed. What is left is not building but **playing**: Experiment 2's first deep dive (DD-1) is blocked on a thin corpus of Experiment-1 replay logs, and without that corpus no further spec can start honestly. The idea is solid, but my interest in playing those sessions faded.

## What I learned

- Keeping the LLM behind a hard boundary (closed vocabulary, deterministic validation, engine-owned dice and log) makes the author swappable: human, template or model.
- Interview-first spec/plan/investigation workflows keep a long project coherent across sessions.
- Real progress was gated by a human playtest, not by code.

## What would make me resume

Wanting to play Experiment 1 myself, or finding one other person who will. Resume point: `docs/context/session-log-13-dd1-corpus-check.md`.

## Where the value lives

`docs/context/llm-game-master.md` (the durable *why*) and `docs/specs/gm-adjudicator.md` (the contract). Treat the documents as the primary artifact.
