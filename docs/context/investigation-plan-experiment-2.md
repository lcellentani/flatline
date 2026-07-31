# Experiment 2 Investigation Plan — "Can an LLM operate the GM?"

| Field | Value |
|-------|-------|
| **Status** | In Progress (0/7 deep dives concluded) |
| **Date** | 2026-07-19 |
| **Scope** | The investigation program that turns the Experiment-2 architecture brainstorm (session log 12) into a startable sequence of SPEC docs |
| **Context** | `docs/context/llm-game-master.md` (vision, methodology), `docs/specs/gm-adjudicator.md` (the implemented contract everything here builds on), `docs/context/session-log-12-experiment-2-investigation-plan.md` (the brainstorm this plan captures) |

---

## 1. Purpose & exit condition

This document is the bridge between conversation-level exploration and the `/spec-new` sequence for Experiment 2 and its adjacent systems. It is **not** an implementation plan — plans derive from Ready specs (`docs/plans/` convention), and this document's purpose is to *produce* the specs. It is process-context, the same family as the session logs.

**The contract, in one line:** each deep dive closes a named set of driving questions and emits artifacts; when every dive feeding a given SPEC is `Concluded`, that SPEC's `/spec-new` interview can run without fabricating a single goal or design decision.

**Exit condition:** the plan is done when the SPEC sequence below (§6) is *startable*, not when the specs are written.

**Kill-switch discipline** (context doc §6): Experiment 1 gates *running* Experiment 2. It does not gate drafting contracts, running feasibility spikes, or designing measurements — a schema-validity spike measures nothing about fun, so nothing here poisons the methodology. The gate bites in two specific places: DD-1 is the evidence audit that keeps the go/no-go honest, and DD-7 is deliberately last because its scenario set and its thresholds both depend on real Experiment-1 session data existing.

## 2. How a deep dive runs (Human~AI protocol)

- **The human brings:** taste calls, first-hand Experiment-1 play experience, the target hardware, and final say on every lock.
- **The agent brings:** drafts of real artifacts (prompts, grammars, schemas, walkthroughs), evidence audits, throwaway spike code, and option analyses that always end in a recommendation — never an unranked survey.
- **Cadence:** each dive session appends a numbered session log (`docs/context/session-log-N-<topic>.md`), same convention as the adjudicator work. Every session ends with an explicit split: *decisions locked* vs. *still open*. A decision made in conversation but not written down does not exist (the failure mode session log 10 was written to fix).
- **Conclusion rule:** a dive is `Concluded` only when every driving question is checked or explicitly relocated (same discipline as spec §5 open-question lists). Exit criteria are falsifiable on purpose — a dive that can't say what "done" looks like isn't a dive, it's musing.
- **Prior material is leanings, not decisions.** Session log 12 records where the brainstorm *leaned* on several questions. A dive re-examines its leanings before locking them; it does not treat them as already decided, and it does not re-derive them from scratch either.

## 3. Deep dive roster

### DD-1 — Game experience target & Experiment-1 evidence audit

| Field | Value |
|---|---|
| **Status** | In Progress |
| **Method** | Evidence audit (replay logs + first-hand session experience) + discussion |
| **Feeds into** | Go/no-go for the whole E2 program; `llm-game-master.md` update; DD-2's template set; DD-7's scenario source |
| **Depends on** | A sufficient Experiment-1 corpus (see Immediate Action #2 in §6) — session-log-13 found the current corpus (one 25-turn 2026-07-18 session, predating `player_input` capture, plus a handful of 1–3-turn stragglers) too thin to run the audit on |

**Driving questions**
- [ ] For every logged Experiment-1 turn: would a Tier-0/1 handler (see DD-2) have resolved it identically, or did it genuinely need a GM? Where does the *fun* cluster — in the un-templatable residue, or spread across everything?
- [ ] Does the fun survive Tier-2 moments being *rare*? (If Experiment 1 showed the fun is specifically in adjudicating everything, latency fights the concept and the genre question reopens — this is the honest residual risk of the whole tiered architecture.)
- [ ] What latency does each action type tolerate? (Budget is per-action-type, not per-game: instant for movement, ~1s for scripted interactions, multi-second acceptable for dialogue replies and rolled dramatic attempts.)
- [ ] Which experience anchor do we target first: classic roguelike (adjudication as the rare high-stakes move), social-deduction cadence (dialogue-dominant, Disco-Elysium-like), with the two-clock BG3 hybrid as the unchanged long-term target?
- [ ] Kill-switch: does the Experiment-1 signal justify proceeding to Experiment 2 at all?

**Prior material:** the user's read that E1 sessions "demonstrated there is potential"; context doc §6/§8. Known handicap: replay logs do not store raw player input (Immediate Action #1) — the audit leans on `interpretation` plus human memory for early sessions.

**Artifacts:** turn classification table; a one-page experience statement; an explicit go/no-go note.

**Exit criteria:** we have clarity when every logged turn is classified, the experience anchor is named, and the go/no-go is written down.

### DD-2 — Tiered action architecture & trigger boundary

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | Design brainstorm + paper prototypes (5–10 real templates for the guardroom) |
| **Feeds into** | Trigger-boundary SPEC |
| **Depends on** | DD-1 (which actions deserve templates) — can start on paper before DD-1 concludes |

**Context.** The brainstorm's central architectural move: the engine already exists — `gauntlet → roll → apply → log` (`gm_turn.hpp`) is a complete deterministic action-resolution engine whose native language is the 6-primitive vocabulary. Scripted actions can be expressed as **precompiled WorkOrders** (pickup = `transfer_item(item, room, player)`, ruling `allow`, no modifiers, authored at design time). Three tiers: **Tier 0** pure engine (movement, physics — no WorkOrder), **Tier 1** precompiled WorkOrder templates (instant, no LLM), **Tier 2** GM-authored WorkOrders (inference fires only here). All tiers converge on the same artifact and the same pipeline; the author is swappable (designer templates / human WoZ GUI / model — three authors, one contract, one gauntlet, one log). The trigger boundary gets a mechanical candidate answer: **the boundary is a template lookup miss.**

**Driving questions**
- [ ] Tier-1 template format: what does a WorkOrder *parameterized over state* look like, and how close can it stay to the JSON the GM emits? Where do templates live, and who authors them?
- [ ] The router rule, precisely: how does an interaction event match (or miss) a template against current state?
- [ ] The Tier-0 line: anything touching *adjudicable* state (inventory, hp, disposition, object properties, flags) goes through the pipeline as a WorkOrder; pure spatial state stays engine-native — is that the right loggability criterion, and does it hold at the edges (per-tile movement would drown the log for zero adjudication value)?
- [ ] Do 5–10 paper templates actually cover the guardroom's obvious actions? What's left in the residue?
- [ ] Is the trigger boundary still movement-coupled (as session log 10 guessed) or now an independent spec? (The lookup-miss framing suggests independent — this revises the earlier guess and should be stated as a revision.)

**Prior material:** session log 12 §2 (tier architecture, lookup-miss boundary, loggability criterion — all leanings).

**Artifacts:** candidate template schema; boundary rule statement; the paper template set.

**Exit criteria:** we have clarity when a named template format expresses the guardroom's scripted actions and the router's match/miss rule is written as a checkable procedure.

### DD-3 — Model-facing turn contract

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | Brainstorm + drafting real artifacts (an actual prompt, an actual grammar sketch) |
| **Feeds into** | E2-1 SPEC |
| **Depends on** | DD-4 (token/latency numbers — the budget must be empirical, not guessed); DD-1 (input channel priorities) |

**Context.** The core division of labor the brainstorm converged on: **the grammar owns form, the prompt owns judgment, the gauntlet owns legality.** The prompt exists to reduce rejection rate, not to guarantee legality — every rule in it is an optimization, which is the liberating constraint when compressing the rulebook into a digest.

**Driving questions**
- [ ] **Decoding rung** — the single biggest fork, three-way: rung 0 free generation + parse (schema-valid rate is an honest measurement of the E4B floor); rung 1 static grammar compiled from the WorkOrder schema (malformed JSON impossible; that metric leaves the benchmark); rung 2 per-turn state-aware grammar (id fields can only decode to ids that exist right now; hallucinated-id rate structurally impossible). Hard ceiling to respect: a grammar constrains vocabulary, not arithmetic or cross-field logic — Σ|modifiers| ≤ 5, `holds()` preconditions, and batch conflicts stay with the gauntlet forever. *Leaning: rung 2 for shipping, with a rung-0 ablation condition in the benchmark ("measure the floor once, then never rely on it").*
- [ ] **Prompt block structure** — an ordered list of named blocks (`RULES` digest, `STATE`, `PLAYER ACTION`, output cue), with a reserved slot for a future `MEMORY` block so semantic memory slots in without renegotiating the contract (the same dormant-hook move `set_flag` made on the output side). What can the digest *omit* because the gauntlet enforces it anyway? How many few-shot examples does the E4B floor need, at ~250–300 tokens each?
- [ ] **STATE serialization** — full JSON vs. curated legible listing. At rungs 0–1, hallucinated-id rate is partly a *prompt design* outcome: how legibly ids are listed changes how often they're misremembered.
- [ ] **Input channel union** — player intent arrives as a structured interaction event, not free text: `{kind, slots…, text?}`. Enumerate the channel set (`use_item_on`, `interact`, `dialogue`, `attack`, …) and its v0 subset. The structured frame does real work for a small model: it never infers *who* from text, only *what and whether*.
- [ ] **Ambiguity policy** — a 2D interaction under-specifies purpose ("use oil_flask on door_north" never said *burn*): commit-to-best-reading, or a disambiguation round-trip (a second inference dependency, the exact cost the one-call decision already rejected once)? *Leaning: commit, with `interpretation` surfaced in the UI before the roll as the correction mechanism.*
- [ ] **Field ordering for streaming** — reorder the WorkOrder schema so mechanical fields decode first and both narrations come last: the harness rolls the die the moment `modifiers` closes, validates and animates the moment the selected branch's effects close, and streams narration as RPG text crawl. Time-to-mechanical-resolution drops from full-generation time to a fraction of it. Field order is a latency design decision E2-1 explicitly owns.
- [ ] **`manner`/tags presentation field** — effects encode outcomes, not manner: "burn the door" and "pick the lock" both resolve to `set_object_state(door_north, locked, false)`, and the *how* lives only in prose today. Candidate fix: an optional closed-vocabulary field (`fire`, `force`, `stealth`, `social`, …) authored by the GM, validated like everything else, consumed by presentation. Additive; decide whether it's in the v0 contract or reserved.
- [ ] **Sampling determinism** — the replay-seed scheme covers the die, not the model. What temperature/seed policy makes a Tier-2 turn replayable?

**Prior material:** session log 12 §2 (worked burn-the-door turn: prompt sketch, expected output, and the failure-mode analysis — including the judgment failure no mechanical metric sees: the guard watching the door burn while the model authors unconditional success).

**Artifacts:** draft prompt v0; draft grammar sketch (static + state-aware variants); field-ordering decision note; channel union sketch.

**Exit criteria:** we have clarity when a complete draft prompt + grammar pair exists for the guardroom, fits DD-4's measured budget, and every checkbox above is a written lean-or-lock.

### DD-4 — Inference runtime feasibility spike

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | Code spike — throwaway, not production; the artifact is the numbers, not the code |
| **Feeds into** | DD-3 (token budget), DD-5 (streaming/speculation design), E2-2's eventual plan Phase 0 |
| **Depends on** | Nothing — start immediately, in parallel with DD-1 |

**Driving questions**
- [ ] Runtime choice: llama.cpp vs. alternatives; Gemma 4 E4B availability as GGUF at Q4 (~3.5 GB, per the context doc's platform bet).
- [ ] Measured on the actual target 8 GB card: generation tokens/sec, prompt-processing speed, time-to-first-token, and end-to-end wall clock for a realistic (prompt, WorkOrder) pair. This replaces every "5–15s, probably" from the brainstorm with data.
- [ ] Constrained decoding: GBNF / JSON-schema support, and the cost of *regenerating* the grammar per turn from `World` state (the rung-2 requirement).
- [ ] Streaming: can the harness act on completed fields mid-generation (the DD-3 field-ordering bet depends on this)?
- [ ] Determinism: does seeded sampling reproduce exactly across runs? Across machines?
- [ ] VRAM headroom alongside a 2D renderer (the context doc's "2D leaves the GPU free" bet, checked with numbers).

**Prior material:** context doc §5 (platform/model bets, the E4B floor).

**Artifacts:** a measured-numbers table; a runtime recommendation; spike code kept out of `src/`.

**Exit criteria:** we have clarity when the numbers table exists and the constrained-decoding + streaming + determinism questions each have a tested yes/no.

### DD-5 — Harness / GameplayDirector architecture

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | Architecture brainstorm + sequence-diagram walkthroughs of the hard cases (cache hit, mispredict, re-prompt loop, gauntlet rejection) |
| **Feeds into** | E2-2 SPEC |
| **Depends on** | DD-2 (router), DD-3 (authoring contract), DD-4 (streaming reality) |

**Context.** The harness is the game's GameplayDirector: it owns `World`, routes interaction events across the tiers, runs the one turn pipeline for every author, and emits applied events to subscribers. The renderer subscribes to **applied effects + roll record + vital transitions**, never to the WorkOrder itself — effects are commands, the applier is the reducer, the log is the event journal, narration is one optional subscriber channel (a bark/caption, demoted from medium to flavor). The log entry already carries exactly what a render feed needs, so a replay viewer through the real renderer is nearly free.

**Driving questions**
- [ ] Director decomposition: router, turn pipeline, event emitter, speculation service — boundaries, ownership of `World`, and what the existing `gm_turn.hpp` already covers vs. what's new.
- [ ] The event emission seam: exact subscriber contract; how narration streaming rides it; where `interpretation` surfaces pre-roll.
- [ ] Speculation cache: keyed `(state-hash, event)`, so staleness is impossible rather than unlikely (the seeded roll makes precomputation safe). Prediction source, invalidation, idle scheduling. NPC-turn speculation is the best case — the world turn's starting state is known the instant the player's turn resolves, so it approaches a 100% hit rate.
- [ ] **Failure policy** (relocated from `gm-adjudicator.md` §5 — this dive is its home): on malformed output (rung-dependent — mostly impossible at rung 2) and on gauntlet rejection: how many re-prompts, feeding back which structured diagnostics (`Stage::BatchConflict` + `conflicting_with_index` was designed so a resubmission "doesn't have to guess" — this is where that investment pays off), then what fallback (`deny` with apologetic narration?), and what the player sees meanwhile.
- [ ] Latency presentation hooks as harness features: the roll ceremony as latency sponge (modifier labels stamping in as they stream), diegetic async for dialogue (the guard visibly considering while inference runs).

**Prior material:** session log 12 §2 (director diagram, four latency techniques, event-stream framing).

**Artifacts:** an architecture note with the sequence walkthroughs.

**Exit criteria:** we have clarity when the four hard-case walkthroughs are written and the failure-policy loop is specified end to end (counts, diagnostics, fallback, player-facing behavior).

### DD-6 — NPC agency & the memory stack

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | Paper walkthrough — the guard through one full world turn |
| **Feeds into** | npc-agency SPEC |
| **Depends on** | DD-2, DD-3. Deliberately non-blocking for the E2-1/E2-2/E2-3 sequence — can trail |

**Context.** NPCs are already half-driven by the GM *reactively*, inside the player's WorkOrder branches (the bribe's failure branch is the guard acting). What's missing is *initiative*. The brainstorm's shape: **the engine picks the goal; the model authors the execution** — a deterministic utility layer (scored from disposition, hp, flags, recent events — all engine-owned state) decides *what the guard wants*; that goal becomes the intent input to the same Tier-2 authoring call, persona and memory in the prompt, gauntlet as backstop. The model stays a translator (the tested bet), never an originator (a new, riskier bet — kept available as a deliberate later escalation, not a default). Containment carries over untouched: a model-driven NPC cannot cheat for the same reasons the GM cannot.

**Driving questions**
- [ ] The goal/execution split, concretely: utility scorer inputs, goal vocabulary, and how a goal becomes a prompt.
- [ ] **The double-driving rule** — the guard currently acts in the player's branches *and* would act on his own turn. Candidate convention to lock: player-turn branches carry only immediate, involuntary reactions; anything deliberate belongs to the NPC's own turn. Cheap to state, confusing to discover late.
- [ ] Memory as the first real `set_flag` consumer: flags feeding both the utility scorer (*failed bribe + witnessed door-burning → suspicion goal*) and the prompt (*the guard remembers the insult*). This is where "the dungeon remembers" gets its first mechanism — one loop, not two features (context doc §4's stack-not-fork claim).
- [ ] The movement gap: which plausible guardroom NPC actions decompose into the existing six primitives (attack, grab, lock, shout all do) and which are gated on the deferred movement system ("walk to the alarm bell").
- [ ] What does `ruling` mean on an NPC turn, where there is no player intent to scope? Does it degenerate to `allow`, or does the schema fork?

**Prior material:** session log 12 §2 (NPC section: reactive-vs-initiative, containment symmetry, origination-vs-translation).

**Artifacts:** the guard world-turn walkthrough (flag state → scorer output → prompt delta vs. a player turn → the WorkOrder that comes back).

**Exit criteria:** we have clarity when the walkthrough exists and the double-driving convention plus the goal/execution seam are written as lockable statements.

### DD-7 — Benchmark & measurement design

| Field | Value |
|---|---|
| **Status** | Not Started |
| **Method** | Brainstorm + metric drafting |
| **Feeds into** | E2-3 SPEC |
| **Depends on** | DD-3 (rung choice reshapes the metric set) + real Experiment-1 session data. **Gated on DD-1's go.** Deliberately last |

**Driving questions**
- [ ] The metric set *per decoding rung*: at rung 2, schema-valid and hallucinated-id rates are 100%/0% by construction and leave the benchmark — what survives is gauntlet-pass rate, batch-conflict rate, re-prompt frequency, per-turn wall clock, and time-to-first-mechanical-field. The rung-0 ablation condition measures the raw floor (schema-valid, hallucinated-id) once.
- [ ] **Unvalidated-branch rule** (relocated from `gm-adjudicator.md` §5 — this dive is its home): gameplay validates only the roll-selected branch; measurement mode validates both, so a hallucination sleeping in the unfired branch counts against the model.
- [ ] Scenario set: harvested `(pre-state, raw player input)` pairs from Experiment-1 replay logs — real player intents systematically out-weird anything invented synthetically, which is exactly what the closed-vocabulary composition bet needs stressing against. Requires Immediate Action #1 to have landed *before* the harvested sessions. Format and versioning.
- [ ] The judgment tier: mechanical metrics cannot see a model that emits flawless work orders and always `deny`s, or authors unconditional success while a guard watches a door burn. Rubric design, with the human GM's work orders from Experiment 1 as the gold reference (same intent, same state, human vs. model — the comparison Experiment 1's logs make possible).
- [ ] Thresholds: which numbers kill, which greenlight. Written before the first run, not after.
- [ ] Reproducibility: fixed seeds (DD-3's sampling policy), versioned scenario sets, versioned prompts/grammars.

**Prior material:** session log 12 §2; `gm-adjudicator.md` §5's relocated unvalidated-branch entry.

**Artifacts:** metric definitions; scenario-set format; threshold proposal.

**Exit criteria:** we have clarity when the metric table (per rung), the both-branch rule, the scenario format, and the thresholds are all drafted.

## 4. Dependency map

```
 start ──► DD-1 (E1 evidence audit) ────────┐
       └─► DD-4 (runtime spike)      ──┐    │  (parallel, immediately)
                                       ▼    ▼
                          DD-3 (turn contract) ◄──► DD-2 (tiers/boundary)
                                       │                 │
                                       ▼                 │
                          DD-5 (director/harness) ◄──────┘
                                       │
        DD-6 (NPC agency) ◄── DD-2+DD-3│   (trails; non-blocking)
                                       ▼
                          DD-7 (benchmark) ◄── DD-1 go + E1 data   (last)
```

## 5. Deep dive → SPEC mapping

| Deep dive | Produces the interview material for | Notes |
|---|---|---|
| DD-1 | *(no spec)* — go/no-go + `llm-game-master.md` update | The kill-switch made procedural |
| DD-2 | **trigger-boundary SPEC** | Revises session log 10's guess: no longer movement-scoped — the boundary is a template lookup miss |
| DD-3 | **E2-1 SPEC** — model-facing turn contract | Prompt blocks, decoding rung, channel union, field ordering, `manner` |
| DD-5 | **E2-2 SPEC** — harness & failure policy | Home of the relocated **failure policy** question |
| DD-6 | **npc-agency SPEC** | Beside, not inside, the E2 sequence; gated on nothing in it |
| DD-7 | **E2-3 SPEC** — benchmark & metrics | Home of the relocated **unvalidated-branch** question |

Spec order of authoring: E2-1 → E2-2 → E2-3, with trigger-boundary insertable after DD-2 concludes and npc-agency whenever DD-6 does. The three relocated questions from `gm-adjudicator.md` §5 now each have a named destination — session log 10's "likely the same future spec" guess is revised: they share a *prerequisite* (the harness), not a *contract*.

## 6. Immediate actions (before any dive concludes)

1. **Capture raw player input in the replay log.** Verified absent: no `player_input` field anywhere in `src/` — the log stores the WorkOrder, whose `interpretation` is the GM's *paraphrase* of intent, a lossy substitute for benchmark replay (the benchmark's input is `(pre-state, player's raw words)`). **Honest tension to resolve, not silently override:** Phase 3 exec planning explicitly *cut* a Player Input panel (`gm-multiturn-loop.md`) because a scratchpad with "no schema field, no persistence, and no logging" had no payoff. The Experiment-2 benchmark is precisely the payoff arriving — the change worth making is the *logged schema field* (plus the minimal GUI input to fill it), which is the part the cut decision said was missing, not a reversal of it. Do this **before** further Experiment-1 play sessions, so they produce benchmark-grade scenario data for free.
2. **Keep playing Experiment-1 sessions.** The logs are DD-1's corpus and DD-7's scenario source; every session played after action #1 lands is double-value.

---

## Status Legend

| Status | Meaning |
|--------|---------|
| Not Started | Dive not yet begun |
| In Progress | At least one session logged against it |
| Concluded | Every driving question checked or explicitly relocated; artifacts exist |
