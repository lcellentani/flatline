# Session Log 12 — Experiment 2: Architecture Brainstorm & Investigation Plan

| Field | Value |
|---|---|
| **Date** | 2026-07-19 |
| **Scope** | Brainstorm the Experiment-2 ("can an LLM operate the GM?") architecture end to end — spec decomposition, model-facing contract, latency, harness shape, NPC agency — and land the result as `docs/context/investigation-plan-experiment-2.md`. No code. |
| **Follows** | `session-log-11-same-batch-effect-conflicts.md` |
| **Reads alongside** | `investigation-plan-experiment-2.md` (the deliverable), `llm-game-master.md`, `gm-adjudicator.md`, `gm-workorder-rulebook.md`, `gm-multiturn-loop.md` |
| **Purpose** | Pick up any deep dive without re-deriving the brainstorm below. The plan doc holds the *program*; this log holds the *reasoning trail* behind it. |

---

## 1. What this session produced

No code changes — documents only.

- **`docs/context/investigation-plan-experiment-2.md`** (new) — the investigation program: 7 deep dives (DD-1…DD-7), each with driving questions, method, artifacts, exit criteria, and a feeds-into SPEC mapping; a dependency map; two immediate actions. A new doc *type* (process-context): it lives in `docs/context/`, not `docs/plans/`, because plans derive from Ready specs and this document's purpose is to produce the specs.
- This log.

## 2. Framing conclusions from the brainstorm

Everything below is a **leaning, not a lock** — each is assigned to a deep dive that re-examines it before any spec records it. Recorded here so no dive starts from zero.

### 2.1 Experiment 2 is three contracts, not one spec

"Can E4B operate the GM" decomposes into: **E2-1** the model-facing turn contract (prompt structure, output constraint), **E2-2** the inference harness + failure policy, **E2-3** the benchmark & metrics. One mega-spec would violate three house disciplines at once: one-contract-per-spec, don't-answer-ahead-of-infrastructure, and interview-first `/spec-new`. The two questions relocated out of `gm-adjudicator.md` §5 in session log 10 split cleanly — **failure policy → E2-2**, **unvalidated-branch → E2-3** — revising log 10's "likely the same future spec" guess: they share a prerequisite (the harness), not a contract. **Trigger boundary** becomes its own spec and is no longer movement-scoped (see 2.3).

### 2.2 The model-facing contract: grammar owns form, prompt owns judgment, gauntlet owns legality

Worked through a full concrete turn (player drags `oil_flask` onto `door_north` in a 2D UI; model answers with the burn-the-door WorkOrder). Key findings:

- **The decoding fork is a three-rung ladder:** rung 0 free generation + parse (schema-valid rate honestly measures the E4B floor); rung 1 static grammar (malformed JSON impossible); rung 2 per-turn *state-aware* grammar regenerated from `World` (id fields can only decode to currently-existing ids — hallucinated-id rate structurally impossible). Hard ceiling: grammars constrain vocabulary, never arithmetic or cross-field logic — modifier budget, `holds()`, batch conflicts stay with the gauntlet forever. Each rung up buys floor reliability, deletes a benchmark metric, adds harness machinery. *Leaning: rung 2 for shipping + a rung-0 ablation in the benchmark.*
- **Input is a tagged union of interaction channels**, not free text: `{kind, slots…, text?}` (`use_item_on`, `dialogue` with free-text payload, …). In the target 2D game, ambiguity relocates from parsing words to *inferring purpose* — the event never said "burn." *Leaning: commit-to-best-reading, with `interpretation` surfaced in the UI pre-roll as the correction mechanism*, rather than a disambiguation round-trip (a second inference dependency, the cost the one-call decision already rejected).
- **The prompt is an ordered list of named blocks** with a reserved future `MEMORY` slot (the same dormant-hook move `set_flag` made on the output side). The prompt exists to reduce rejection rate, not guarantee legality — every rule in it is an optimization, which liberates the digest compression.
- **The judgment failure no mechanical metric sees:** the worked example's model output was mechanically flawless *and* GM-poor (unconditional success on burning a door while the guard stands watching). Judgment evaluation is tier-2, benchmark-side (DD-7), with Experiment-1 human work orders as gold reference.

### 2.3 The tier architecture: the engine already exists; the LLM is its escalation path

The latency worry ("5–15s per action") dissolves once the hidden assumption — every action goes through the LLM — is dropped. The 6-primitive vocabulary, designed as the GM's containment language, doubles as the engine's native action representation: **scripted actions are precompiled WorkOrders** run through the same `gauntlet → roll → apply → log` pipeline. Three tiers: Tier 0 pure engine (movement/spatial), Tier 1 precompiled templates (instant, no LLM — the Brogue tier), Tier 2 GM-authored (inference fires only here). All tiers converge on one artifact and one pipeline; **the author is swappable** (designer / human WoZ GM / model) — generalizing the spec's "the GM is a swappable component" seam. The trigger boundary gets a mechanical candidate: **a template lookup miss**. The game is fully playable with the LLM off; a game needing inference on every action would be evidence of drift back toward the freedom-maximizing crowd the cage thesis rejects.

### 2.4 Latency techniques for when Tier 2 fires

Four, in value order: **(1) WorkOrder field reordering + streaming** — mechanical fields first, narrations last; roll the moment `modifiers` closes, validate/animate the moment the branch's effects close, stream narration as RPG text crawl; field order becomes an E2-1 contract decision. **(2) The roll ceremony as latency sponge** (BG3 d20 precedent; modifier labels stamp in as they stream — "the GM pauses to think" made diegetic). **(3) Speculative pre-adjudication** during player deliberation, cached by `(state-hash, event)` — the seeded roll makes precomputation safe, the hash key makes staleness impossible. **(4) Diegetic async for dialogue** — the guard visibly considers while inference runs. Latency budget is per-action-type, not per-game.

### 2.5 Effects are the machine-readable answer; prose is one subscriber

The WorkOrder is already action-first: applied effects + roll record + vital transitions are structured data a renderer translates directly (item arcs, hit flashes, door state swaps, collapse animations). The harness's event emitter feeds subscribers (renderer, audio, UI, future memory); narration demotes from medium to an optional bark/caption channel. The log entry already carries what a render feed needs — a replay viewer through the real renderer is nearly free. **One real gap:** effects encode outcomes, not *manner* — "burn" vs. "pick the lock" produce the same `set_object_state`; candidate fix is an optional closed-vocabulary `manner`/tags field (E2-1 question, additive).

### 2.6 The harness is a GameplayDirector

Router (tier dispatch) + turn pipeline (`gm_turn.hpp`, already built) + event emitter + speculation service, owning `World`. Matches the user's GameManager/GameplayDirector mental model; the genuinely new parts are the router, the cache, and the emission seam — the pipeline core exists.

### 2.7 NPC agency: engine as will, model as voice and hands, gauntlet as physics

NPCs are already GM-driven *reactively* (inside player-turn branches — the bribe's failure branch is the guard acting); what's missing is *initiative*. Shape: **the engine picks the goal** (deterministic utility scorer over disposition/hp/flags/recent events), **the model authors the execution** (same Tier-2 authoring call, persona + memory in prompt, gauntlet as backstop) — keeping the model a *translator* (the tested bet), never an *originator* (a new, riskier bet, kept as a deliberate later escalation). Containment carries over: an LLM-driven NPC cannot cheat for the same reasons the GM cannot. NPC turns are the best speculation case (world-turn start state is known the instant the player's turn resolves — near-100% cache hit). Memory becomes the first real `set_flag` consumer, cashing out the context doc's stack-not-fork claim. Two gaps: the **movement primitive** (first genuine need is NPC initiative beyond one room) and the **double-driving rule** (candidate convention: involuntary reactions in player branches; deliberate acts on the NPC's own turn).

## 3. Gaps surfaced (named, not fixed)

- **`player_input` absent from the replay log** — grep-verified across `src/`: the log stores the WorkOrder, whose `interpretation` is the GM's paraphrase — lossy for benchmark replay, which needs `(pre-state, raw player words)`. Interacts with a prior decision: Phase 3 exec planning *cut* a Player Input panel because an unlogged scratchpad had no payoff (`gm-multiturn-loop.md`). The E2 benchmark is the payoff arriving; the fix is the logged schema field the cut said was missing, not a reversal. **Immediate Action #1** in the plan — land it before further Experiment-1 sessions.
- **`manner`/presentation gap** (§2.5) — assigned to DD-3.
- **Movement primitive gap** (§2.7) — scoping assigned to DD-6; the movement system itself stays deferred per spec Non-Goals.
- **Double-driving convention unstated** (§2.7) — assigned to DD-6 to lock.

## 4. Deliberately not decided this session

The decoding rung, the ambiguity policy, the channel set, the Tier-1 template format, the failure-policy loop, all metrics/thresholds, and the game's experience anchor. Each is a driving question in a named dive; the leanings in §2 are inputs to those dives, not decisions — per the plan's own protocol ("prior material is leanings, not decisions").

## 5. Suggested next step

Start **DD-1** (Experiment-1 evidence audit — the kill-switch made procedural) and **DD-4** (runtime spike — the only purely empirical dive) in parallel; neither depends on anything. Before the next Experiment-1 play session, land **Immediate Action #1** (`player_input` in the log schema + minimal GUI input), so every session played from now on is benchmark-grade data for DD-7.
