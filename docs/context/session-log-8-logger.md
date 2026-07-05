# Session Log 8 — GM Adjudicator: The Logger

| Field | Value |
|---|---|
| **Date** | 2026-07-02 |
| **Scope** | Build the logger — the last unbuilt seam named in session log 7 §4/§5, closing the full Wizard-of-Oz loop. |
| **Follows** | `session-log-7-roll-scheme.md` |
| **Reads alongside** | `gm-adjudicator.md` (spec), `gm_effects.hpp`, `gm_state.hpp`, `gm_gauntlet.hpp`, `gm_applier.hpp`, `gm_roll.hpp`, `gm_json.hpp`, `gm_log.hpp` (new), `wizard_of_oz_demo.cpp`, `test_log.cpp` (new) |
| **Purpose** | Pick the harness build back up without re-deriving anything below. |

---

## 1. What this session produced

New artifacts:
- `gm_log.hpp` — `Outcome = std::variant<ModifierBudgetRejected, GauntletRejected, Applied>`, `LogEntry{turn_number, work_order, outcome}`, JSON serializers following `gm_json.hpp`'s existing tag-dispatch recipe, `append_to_file`/`read_log` (JSON Lines), and `replay_matches(Applied)` — the actual replay proof, not just storage.
- `test_log.cpp` — verification harness for this session's change, not a shipped project artifact. 27 checks: JSON round-trip for all three `Outcome` kinds (including the rolled/no-roll variant of each where applicable), `replay_matches` both succeeding on a faithful entry and correctly rejecting a deliberately tampered one, and `append_to_file`/`read_log` JSONL round-tripping across multiple entries — including an explicit check that a third append leaves the first two lines untouched (genuinely additive, not a rewrite).

Modified artifacts:
- `wizard_of_oz_demo.cpp` — the `[LOG]` seam, marked unbuilt since session log 3, is now real: one `append_to_file` call at each of the turn's three possible exit points (modifier-budget rejection, gauntlet rejection, applied), reusing a single `std::optional<log::RollInfo>` computed once per turn rather than re-deriving it per branch. Header comment updated to reflect the seam closing.

## 2. Decisions locked this session

### 2.1 Approach B: full per-turn snapshots, not deltas-only replay-from-genesis
Weighed against replaying strictly from the turn-0 world by folding each turn's `applied_effects` forward, and a hybrid (genesis snapshot + a cheap `fnv1a` hash of each turn's world as a checksum). Locked B — every `Applied` entry carries both `world_before` and `world_after`, so any single logged turn is independently replayable/verifiable without first replaying every turn before it. Named cost, accepted rather than hidden: `world_after[n] == world_before[n+1]`, so every world state is stored twice. Immaterial at this room's size; flagged as the thing to revisit if session length grows enough that storage becomes real (the hash-checksum hybrid is the parked alternative if that happens).

### 2.2 One `LogEntry` per turn, not per gauntlet stage
The gauntlet's internal per-effect stages (ids-exist → preconditions → magnitude-bounds) never get their own log line — they collapse into three plain fields (`effect_index`, `stage`, `reason`) on a single `GauntletRejected` outcome. Directly follows from the already-locked "one `WorkOrder` call per turn" architecture (`gm_effects.hpp`): the gauntlet running several internal checks doesn't make it several turns, it's one atomic verdict with a multi-step justification. Analogy used in discussion: a database transaction log writes one entry per commit/abort, not one per row-lock check inside the transaction.

### 2.3 `Outcome` as a `std::variant`, not a flat struct with "present only if" fields
The initial sketch had every conditional field (`applied_effects`, `world_before`, `world_after`, `roll`) sitting flat on one struct, governed only by a comment. Rejected once named explicitly: a flat struct compiles fine even when something reads `world_after` on a rejected turn — same shape of loophole as `Damage.via` being optional-with-a-silent-bypass before it was made required-with-a-sentinel. Locked the variant instead: `ModifierBudgetRejected`, `GauntletRejected`, and `Applied` each carry only the fields that outcome can actually have. `ModifierBudgetRejected` in particular carries no `roll` field at all — not an absent optional, structurally absent — because the pipeline never reaches the roll before that gate can fire. Same reasoning `gm_gauntlet.hpp` already used splitting `Failure` from `ModifierBudgetFailure` rather than folding them into one type.

`ModifierBudgetRejected` keeps `magnitude_sum` and `budget` alongside `reason` (mirroring `gauntlet::ModifierBudgetFailure`'s own fields) rather than flattening to a reason string — a replay reader gets "rejected, was 8 against a budget of 5," not just "rejected."

### 2.4 New serializers live in `gm_log.hpp`, not `gm_roll.hpp`/`gm_gauntlet.hpp`
Two reasons, discussed before implementation: `gm_roll.hpp` already includes `gm_json.hpp` for the seed's canonical dump, so the reverse include would cycle; and `gm_roll.hpp`'s own header comment leans on `RollResult` being a pure, I/O-free struct, which bolting a serializer onto it would quietly break. `gm_log.hpp` sits downstream of everything and is the natural home for log-event-specific JSON without touching either header's existing contract.

**Bug caught during implementation, not before it:** `RollResult`'s serializer was first written as a plain function inside `namespace gm::log`. It compiled, but nlohmann's `to_json`/`from_json` hooks are found via ADL on the *type's own* namespace (`gm::roll`), not wherever the function happens to be textually defined — so it silently never fired. Fixed by reopening `namespace gm::roll` from within `gm_log.hpp` to add the serializer there instead. This is additive (doesn't edit `gm_roll.hpp`'s file, doesn't give the roll header a new dependency) and is the standard idiom for hooking ADL from a downstream header — but it's a real category of mistake worth naming: "which file a function lives in" and "which namespace it's found in" are different questions, and nlohmann's ADL only cares about the second.

### 2.5 Replay comparison by JSON equality, not `operator==` on `World`
`World`/`Entity`/`Player`/`ObjectState` have no equality operators, and none were added. `replay_matches` compares `json(replayed) == json(applied.world_after)`, reusing the exact trick the demo's own round-trip proof already uses (`json::parse(...) == json::parse(...)`) rather than hand-rolling equality across every state struct for one check.

### 2.6 File format: JSON Lines, genuinely append-only
`append_to_file` opens in append mode and writes one `json(entry).dump()` line per call. Matches the spec's own verb ("log... as one replayable event") more literally than a single growing JSON array would, and survives a crash mid-session with every prior line intact — `test_log.cpp` proves this directly rather than assuming it (§3).

## 3. Verification performed

- Fresh sandbox: downloaded `nlohmann/json` v3.11.3 single-header (matching the version already pinned in `gm_json.hpp`'s header comment) — not present by default.
- `gm_log.hpp` compiled standalone (`-fsyntax-only`, **C++20**, `-Wall -Wextra`) — clean, only the same pre-existing benign `-Wmissing-field-initializers` warnings from `gm_effects.hpp`'s no-failure-branch examples. Caught and fixed the ADL placement bug (§2.4) during this step, before it reached `test_log.cpp`.
- `test_log.cpp` compiled and run: **27/27**, including round-trip fidelity for `Applied` (rolled and no-roll), `ModifierBudgetRejected`, and `GauntletRejected` (rolled and no-roll); `replay_matches` both confirmed true on a faithful entry and confirmed false on a deliberately corrupted `world_after` (proves the check isn't a tautology); and `append_to_file`/`read_log` round-tripping three sequential appends with order and content intact.
- Baseline regression, unchanged: `test_gauntlet` 11/11, `test_applier` 15/15, `test_roll` 20/20 — matching session log 7 exactly, confirming `gm_log.hpp` didn't alter any prior header's behavior.
- `wizard_of_oz_demo.cpp` rebuilt and run end-to-end with `[LOG]` wired for real: this run's fixed `turn_number = 1` reproduced session log 7's own result (die 1, total 6 vs. DC 15, failure), the gauntlet applied the failure branch (guard disposition −1), and exactly one `LogEntry` was written to `woz_log.jsonl`. Exit code 0.
- **The actual goal named in session log 7 §5**, checked directly rather than assumed: a *separate* process (not the demo, not `test_log.cpp`) loaded `woz_log.jsonl` from disk via `log::read_log` and called `log::replay_matches` on the entry it read back — **true**. This is "replaying a logged run from disk reproduces the identical `World` transition," the property session log 7 distinguished from the weaker "same inputs produce the same roll" (already proven that session).

## 4. Remaining pieces for a full Wizard-of-Oz loop

- **Validator / gauntlet control flow** — ✅ built session 5.
- **Applier** — ✅ built session 6.
- **Seeded roll** — ✅ built session 7.
- **Logger** — ✅ **built this session.**

All four seams marked unbuilt since session log 3 are now closed. Experiment 1's Wizard-of-Oz loop is feature-complete end to end: bridge → modifier budget → roll → gauntlet → apply → log, one turn, fully replayable from a file on disk.

## 5. Suggested next step

Two candidates, neither blocking, both already named as debt in prior logs:

1. **Spec reconciliation.** `gm-adjudicator.md` §5 still lists "modifier → roll scheme" as an open question and credits neither `gm_roll.hpp` nor this session's `gm_log.hpp` — the drift session log 7 flagged (itself repeating what session log 4 flagged, and what session log 2 fixed once already) has now gone two sessions without a pass. Worth doing before it compounds further, and now covers more ground than a quick edit: the roll scheme, the modifier budget, and the entire logger design (Approach B, the `Outcome` variant shape, JSONL) are all real content the spec doesn't reflect yet.
2. **A real multi-turn loop.** The demo still runs exactly one hardcoded turn (`turn_number = 1`, one fixed `WorkOrder` read from disk). With every seam now built, the natural next step is a genuine REPL — read player intent, GM authors a `WorkOrder`, run the full pipeline, repeat — which would also be the first real test of whether Approach B's per-turn storage cost (§2.1) is felt in practice, and the point where `turn_number` needs to become an actual counter instead of a constant.

*Noticed, not touched this session:* `World`/`Entity`/`Player`/`ObjectState` still have no `operator==` (§2.5) — a deliberate choice (JSON equality does the job every place it's been needed so far), not an oversight. Flagging so it isn't "fixed" reflexively later without checking whether anything actually needs it.
