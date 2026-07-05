# LLM Game Master — Project Context

| Field | Value |
|-------|-------|
| **Status** | Living document |
| **Date** | 2026-07-01 |
| **Scope** | Shared background for all specs under this project |

---

## Purpose

This document holds the durable *why* behind the project — vision, novelty, platform bets, and methodology — so individual specs (e.g. `gm-adjudicator`) can stay focused on a single contract and reference this instead of re-deriving it. If a fact here changes, it changes once, here.

## 1. Vision

Place an LLM at the *core* of a roguelike as a game master: it interprets open-ended player intent and shapes the experience while operating inside a fixed set of rules. The goal is explicitly **not** to compete on art, pace, or technical spectacle. The bet is that fun can emerge from an unconventional approach — a world that interprets what you *try*, not just what it was pre-authored to allow.

## 2. Why this is novel (against what already ships)

The broad idea is an active frontier as of mid-2026, not virgin territory: AI Roguelite (and its 2D spinoff), Instantale (Epic, June 2026), and several on-device hobby builds all put an LLM at a roguelike's core. Nearly all make the *same* move — maximal freedom, the AI generates everything, the player types anything — and hit the same wall AI Dungeon hit: infinite freedom dissolves into mush with no stakes.

Our inversion is the under-explored slice: **keep the rules hard and let only specific gates be soft.** The roguelike skeleton (permadeath, turn scarcity, a closed room set) is not the canvas; it is the *cage* that turns LLM freedom into a game. Differentiation has two stages:

- **Now:** a smart referee inside hard constraints — closer to a real game than the freedom-maximizing crowd.
- **Later:** a dungeon that *remembers* and reacts in fuzzy, narrative terms (semantic memory) — the thing the crowd doesn't do, and where "it feels alive" lives.

## 3. The combinatorial-richness problem (design lineage)

Roguelikes generate "try anything" depth in two classic ways; ours is a third.

- **NetHack — breadth by labor.** Thousands of hand-authored interaction cases (the emergent "interaction matrix"; the community calls the ethos TDTTOE, "The DevTeam Thinks of Everything"). Rich and beloved, but it scales only with human effort and goes blank on genuinely un-anticipated input.
- **Brogue — breadth by composition.** A handful of orthogonal, physics-like systems (fire spreads through grass, gases pool and flow, water/lava/caustic interact) whose combinations multiply into depth — with no AI at all. This sets the bar the LLM must clear.
- **Ours — breadth by inference.** Generate the interaction at runtime, semantically.

The LLM only earns its place on the cell both others leave blank — not "more interactions" (Brogue disproves the need for AI there) but interactions over things you *cannot* reduce to physics or a finite case list: **intent, persuasion, lies, social reasoning, and semantic memory.**

## 4. The three GM roles

- **Bard** (narration) — read-only over already-committed state; lowest risk, least "core."
- **Director** (semantic dungeon assembly) — picks narrative beats while a deterministic solver guarantees a connected, solvable layout. Out of scope for v0.
- **Adjudicator** (resolves creative actions) — the only role that *gates a state transition* rather than decorating one. This is what "the LLM at the core" actually means, and it is our first bet.

Adjudication and semantic memory are a **stack, not a fork**: adjudication is the verb; memory is the world reacting to the accumulated verbs (memory becomes an *input* to the adjudicator on the next turn). One GM loop, not two competing features.

## 5. Platform & model bets

- **2D** — sidesteps the failure mode that has killed on-device 3D attempts, where the renderer and the LLM fight over the same VRAM. A 2D game leaves the GPU essentially free for inference.
- **Turn-based** — absorbs LLM latency: a turn already costs the player seconds of deliberation, and that idle time hides generation.
- **On-device, small model** — target Gemma 4 **E4B** (~3.5 GB at Q4, runs on an 8 GB card; Apache 2.0, so commercial use is unencumbered).
- **The floor is real.** ~3.8B parameters is roughly the smallest size that reliably emits valid structured output; smaller models fall apart on formatting. E4B sits *at* that floor — workable, but marginal. This is precisely why "can the model do it?" is a genuine open risk rather than a formality (see Methodology).

## 6. Methodology — two experiments, one kill-switch

The two questions are orthogonal and must not be conflated.

- **Experiment 1 — "Is it fun?"** A human plays the GM behind the validator (the *Wizard of Oz* method: fake the one hard organ, build everything around it for real). Tests the *experience*. Requires no inference.
- **Experiment 2 — "Can E4B do it?"** Build the real harness; the question flips to a measurable engineering benchmark — schema-valid rate, hallucinated-id rate, re-prompt frequency. No taste required, just numbers.

**Kill-switch logic:** if the loop isn't fun with a fluent, infinitely capable human behind the curtain, a 4B will never rescue it. So Experiment 1 *gates* Experiment 2. Conflating them poisons the result — a stutter would leave you unable to tell whether the *idea* or the *model* was the weak link.

Note: a coded prototype in which a human still supplies the rulings **is** Experiment 1. It fakes the *oracle*, not the *infrastructure* — which is exactly the right thing to fake.

## 7. Turn the model's defects into theme

Hallucination and inconsistency are the top reliability risk — and a game is a domain where being wrong is often *fine* (a weird mob name is funny; off-kilter dialogue adds charm). Framed as an unreliable, dreamlike, trickster dungeon, the #1 blocker becomes flavor. Deterministic procgen cannot hallucinate; in research mode, that defect is raw material.

## 8. Long-term target — the BG3 hybrid

The finished experience can run **two clocks**, BG3-style: free real-time exploration (pure deterministic engine, no LLM) that hard-switches into turn-based, adjudicated interaction at flagged trigger points. Nothing in the adjudicator contract fights this — the contract is identical whether the player walked to the door freely or was placed there by a generator.

One cost to record: free movement weakens the **latency-hiding trick** available in a pure turn-based maze (speculatively pre-generating content for the 2–4 adjacent rooms while the player deliberates, hiding generation behind their hesitation). Under free movement you can't cheaply predict the next interaction, so you lean harder on "the GM pauses to think" being diegetic. This is why the hybrid is a *target*, not a v0 feature.
