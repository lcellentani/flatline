# Session Log 13 — DD-1 corpus check

| Field | Value |
|---|---|
| **Date** | 2026-07-31 |
| **Scope** | Attempted to open DD-1 (Game experience target & Experiment-1 evidence audit) from `docs/context/investigation-plan-experiment-2.md`; determined the current Experiment-1 replay-log corpus is insufficient to run the audit. |
| **Follows** | session-log-12-experiment-2-investigation-plan.md |
| **Reads alongside** | `docs/context/investigation-plan-experiment-2.md` (§3 DD-1, §6 Immediate Actions), `docs/specs/player-input-logging.md`, `docs/plans/player-input-logging.md` |
| **Purpose** | Pick the work back up without re-deriving anything below. |

---

## 1. What this session produced

No audit artifacts — documents only. Checked on-disk replay logs in `build/bin/`:

- `gui_session_log.jsonl` — 25 turns, dated 2026-07-18. The only substantial Experiment-1 dataset. Predates `player_input` logging (landed 2026-07-30 per `docs/specs/player-input-logging.md`), so every turn carries only the GM's `interpretation` paraphrase, not the player's raw words.
- `gui_session_log-20260721-171640-360.jsonl` — 1 turn.
- `gui_session_log-20260730-202342-674.jsonl`, `gui_session_log-20260730-203554-260.jsonl` — 2 turns each, dated right around the `player_input` feature landing; provenance (real short session vs. smoke test of the new field) not confirmed with the human this session.
- `woz_log.jsonl` — 3 turns, via `woz_loop.cpp` (not the live-session front end per `CLAUDE.md`).

Confirmed via `docs/specs/player-input-logging.md` / `docs/plans/player-input-logging.md`: Immediate Action #1 (capture raw `player_input` in the replay log) is **Implemented/Done** as of 2026-07-30.

## 2. Decisions locked this session

1. **DD-1's evidence audit does not start this session.** The only sizeable corpus (`gui_session_log.jsonl`, 25 turns) is a single session that predates raw-player-input capture, and every other log file is 1–3 turns of unconfirmed provenance. Running the turn-classification audit (driving question 1) or naming an experience anchor (driving question 4) on this data would mean fabricating signal the plan's own conclusion rule forbids. Alternative considered — audit the 07-18 session alone, using `interpretation` as a lossy stand-in for player intent — rejected by the human, who wants a better corpus before spending the audit.
2. **DD-1 status moves to `In Progress`.** Per the plan's status legend ("At least one session logged against it"), even though no driving question was closed or relocated.

## 3. Leanings & still open

All 5 DD-1 driving questions remain fully open — none were attempted, so none carry even a partial lean this session.

Open, not decided: whether the two 2026-07-30 two-turn logs are real short play sessions or smoke tests of the `player_input` field. The human's answer ("we need better session logs") didn't resolve this directly one way or the other — read here as "insufficient either way," not as a classification of those specific files.

## 4. Gaps surfaced (named, not fixed)

- The plan's DD-1 row states "Depends on: Nothing — start immediately." In practice, a sufficient Experiment-1 corpus is an unstated dependency — the plan's own Immediate Action #2 ("Keep playing Experiment-1 sessions... every session played after action #1 lands is double-value") already anticipated this, but the DD-1 dependency row doesn't cross-reference it. Not fixed this session (a wording/cross-reference change to the plan, not a blocker); flagging so the next reader doesn't hit the same surprise DD-1 just did.

## 5. Status changes applied

- `docs/context/investigation-plan-experiment-2.md` — DD-1 **Status**: Not Started → In Progress.

## 6. Suggested next step

Play more Experiment-1 sessions now that `player_input` logging has landed — every session from here captures raw player words alongside the GM's paraphrase, i.e. full benchmark-grade data per the plan's own framing. Once a few such sessions exist (plus a decision on whether the thin 07-21/07-30 logs count), re-open DD-1 for the actual evidence audit.
