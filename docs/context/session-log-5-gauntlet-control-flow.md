# Session Log 5 — GM Adjudicator: Spec Alignment & Gauntlet Control Flow

| Field | Value |
|---|---|
| **Date** | 2026-07-02 |
| **Scope** | Close session log 4's flagged spec drift; lock and build the validator gauntlet control flow — the sole remaining blocker session log 4 identified on the critical path. |
| **Follows** | `session-log-4-validator-reference-data.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_effects.hpp`, `gm_state.hpp`, `gm_rules.hpp`, `gm_gauntlet.hpp`, `gm_json.hpp`, `wizard_of_oz_demo.cpp` |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

New artifacts:
- `gm_gauntlet.hpp` — the gauntlet control flow. `run_gauntlet(world, branch)` takes one already-selected effect batch and walks it through ids-exist → preconditions → magnitude-bounds, stage by stage, per effect, in order. One free function per (effect type, stage) in a `detail` namespace, dispatched via `std::visit`, so the dispatch reads as a checklist against the spec's own gauntlet stage list rather than one large branching function.
- `workorder_hallucinated.json` — JSON fixture reproducing the spec's "hallucination (deny at the gauntlet)" worked example, for an end-to-end disk → bridge → gauntlet demo run rather than only the hardcoded C++ literal.
- `test_gauntlet.cpp` — verification harness for this session's change, not a shipped project artifact. 11 checks: both worked examples, fail-fast effect-index ordering, both sentinel carve-outs, damage clamping, and the two eligibility checks in §2.3 below.

Modified artifacts:
- `gm-adjudicator.md` — two separate passes this session:
  - **(a) Closed session log 4's flagged drift.** Contract Schemas now credits `gm_json.hpp` alongside `gm_effects.hpp`/`gm_rules.hpp`; removed the stale "near-term note" describing the JSON serialization bridge as unbuilt tooling — it shipped in session log 3 and had just never been cleaned up.
  - **(b) Gauntlet control flow decision.** New locked Key Decision ("Gauntlet validates only the roll-selected branch; fails fast per effect"); new Open Question naming the resulting gap in Experiment 2 stats (see §2.1).
- `gm_rules.hpp` — added `entity_exists`/`object_exists`/`item_exists` (the ids-exist stage's actual lookups — previously only documented as a carve-out *note*, never coded) and `resource_eligible` (a gap discovered while implementing; see §2.3). Pure additions — nothing pre-existing was touched.
- `wizard_of_oz_demo.cpp` — the `[VALIDATOR]` seam, a TODO comment since session log 3, is now a real `run_gauntlet` call against `effects_on_success` (documented in-line as a stand-in for "whichever branch the still-unbuilt roll will eventually select"). Added a second demo block running `workorder_hallucinated.json` end-to-end and asserting it's rejected.

## 2. Decisions locked this session

### 2.1 Gauntlet validates one branch, fails fast (A1 + B1)
Two independent axes were brainstormed:
- **When does validation run relative to the roll?** A1 (validate only the branch the roll picks) vs. A2 (validate both branches always, before rolling).
- **Stop at first violation, or collect all?** B1 (fail-fast, one `Failure`) vs. B2 (exhaustive, a list of violations).

**Locked: A1 + B1.** Cheapest path to a running Experiment 1 loop, matches the spec's literal wording ("every effect in the *applied* branch is validated"), and is stylistically consistent with `gm_json.hpp`'s existing throw-on-first-bad-thing pattern. *Alt rejected:* A2/B2 — more accurate for Experiment 2's failure-rate metrics, but pure overhead before that benchmark exists; not a one-way door, revisit when Experiment 2 tooling is built.

**Named gap, tracked in the spec (§5 Open Questions):** under A1, the branch the roll does *not* select is never validated at all. Harmless for gameplay (an unfired branch never applies) but will understate Experiment 2's hallucinated-id/schema-valid rates if measured naively later.

### 2.2 Result shape: `std::variant<vector<Effect>, Failure>` (Option B)
Considered a struct with an `optional<Failure>` field (Option A) vs. a variant (Option B). **Locked: Option B.** "Ignore `applied` on a failure" becomes unrepresentable rather than a convention to remember at every call site — the same fix shape as `Damage.via` going from optional-with-a-silent-bypass to required-with-a-sentinel (this project's own standing top lesson on schema design).

### 2.3 `resource_eligible()` — gold is player-only
Surfaced while implementing `ModifyResource`'s precondition stage, not previously named: `gold` exists only on `Player`, not `Entity` (`gm_state.hpp`), so `modify_resource(guard_01, "gold", -5)` would have passed every existing check and had no field to modify at apply time — the mirror image of `disposition_eligible`'s already-locked player-exclusion gap. Closed inline (judged a direct application of an already-locked pattern, not a new design fork) by adding `resource_eligible(world, id, resource)`, same shape as `disposition_eligible`: `hp` is eligible on both `Player` and `Entity`; `gold` only on `Player`.

**Not touched, still deferred:** `modify_resource(hp)` has no floor/ceiling check — same pre-existing spec gap ("hp floor with no ceiling"), untested by the guardroom scenario either way.

## 3. Verification performed

- Compiled `gm_rules.hpp` standalone under **C++20** (`-Wall -Wextra`) after adding the three existence helpers and `resource_eligible` — clean, only the pre-existing benign `-Wmissing-field-initializers` warnings from `gm_effects.hpp`'s no-failure-branch examples (unchanged since session log 3).
- Compiled `gm_gauntlet.hpp` standalone under the same flags — same clean result.
- Built and ran `test_gauntlet.cpp`: **11/11 passed**, including fail-fast ordering (a 3-effect batch with a hallucination at index 1 reports index 1, not 0 or 2), both sentinel carve-outs (`VOID_ENTITY`, `UNARMED_ITEM`), damage clamping against the cap table, and a constructed damage-cap **table gap** (`torch`, an item present in state but absent from `kDamageCap`) correctly reported distinctly from a hallucination, per session log 4 §2.1's discipline.
- Compiled and ran `wizard_of_oz_demo.cpp` end-to-end: state prints, `workorder_bribe.json` parses, the gauntlet accepts all 3 effects on the success branch, round-trip is identical, the closed-vocabulary gate still rejects `teleport`, and the new `workorder_hallucinated.json` run is correctly rejected by the gauntlet (not just the bridge). Exit code 0.

## 4. Remaining pieces for a full Wizard-of-Oz loop

Updated from session log 4's list — the sole blocker it named is now cleared:

- **Validator / gauntlet control flow** — ✅ **built this session.**
- **Applier** — unbuilt. Now the sole remaining blocker on the critical path: apply a gauntlet-approved `vector<Effect>` to `World` atomically.
- **Seeded roll** — unchanged; still depends on the open **modifier → roll scheme** question (spec §5). Not a blocker for the applier — the applier just needs *a* gauntlet-approved branch to apply, the same "don't need the real roll yet" reasoning session log 4 used for the gauntlet.
- **Logger** — unchanged, unbuilt.

## 5. Suggested next step

**Build the applier.** It's the new sole blocker, and — same shape as last session's reasoning — it doesn't need the roll scheme either: it can be built and tested by applying the gauntlet's `applied` output for `bribe_the_guard`'s success branch, its failure branch, and `burn_the_door`'s success branch directly to a `World`, and asserting the resulting state (gold spent, key transferred, disposition shifted, door unlocked, flask consumed) by hand. The roll only decides *which* branch reaches the applier, not what the applier does with it.

*Noticed, not touched this session:* the Contract Schemas section credits `gm_gauntlet.hpp` nowhere, since it's control flow rather than a schema — same treatment `wizard_of_oz_demo.cpp` already gets. Not a gap, just flagging the reasoning so it isn't second-guessed later.
