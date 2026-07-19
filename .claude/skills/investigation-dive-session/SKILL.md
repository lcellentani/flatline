---
name: investigation-dive-session
description: Run one deep dive session from an Investigation Plan — the Human~AI collaboration protocol for closing a dive's driving questions and producing its artifacts, ending with a mandatory closing ritual (locked-vs-open split, numbered session log, status updates). Use whenever the user wants to start, resume, or continue work on a deep dive (DD-N) from an Investigation Plan document. Not user-invocable directly — used internally by the /investigation-dive command.
user-invocable: false
---

# investigation-dive-session

Runs one session against one dive of a `docs/context/investigation-plan-<slug>.md`. A session's job is to move the dive's driving questions from open to checked-or-relocated and to land its artifacts — and to leave a written trail either way.

## Preconditions

- The Investigation Plan and the named dive must exist. If the dive is ambiguous, list non-concluded dives with their dependency status and recommend one — don't pick silently.
- Check the dive's **Depends on** row. If a dependency isn't `Concluded`, say so and proceed only if the human explicitly accepts working ahead of it.
- If an **External gate** in the plan's §1 gates this dive, stop and say so — working a gated dive is the human's call to make knowingly, never a default.

## Opening the session

1. Restate the dive's driving questions, current status, and prior material. **Prior material is leanings, not decisions** — name the leanings as leanings. Never re-derive something a session log already records; link it instead.
2. Agree the session's scope: which questions this session attacks (all of them is a valid answer for a small dive).

## Division of labor

- **The agent brings:** drafts of real artifacts (not descriptions of artifacts), evidence audits with the sources read, spike code kept out of production paths, and option analyses that always end in a recommendation — never an unranked survey.
- **The human brings:** taste calls, first-hand evidence, and final say on every lock. Do not poll the human on anything verifiable in the repo — go verify, then present the finding.

## During the session

- Label every conclusion as **lean** or **lock** at the moment it's reached — ambiguity here is what the closing ritual exists to catch.
- Write artifacts to the paths the dive names. An artifact that exists only in conversation doesn't exist.
- If a question turns out to belong to another dive or a future deliverable, **relocate it explicitly** — name the destination in the plan and the session log. Never answer another dive's question in passing without recording it there, and never silently drop one.
- If something out-of-scope is discovered broken or stale, flag it in the session log's "gaps surfaced" — don't fold unrelated fixes into the session.

## Closing ritual (mandatory, in order — the session is not over until this has run)

1. **Present the split:** decisions locked this session vs. still open — explicitly, to the human, before writing anything.
2. **Append the session log:** `docs/context/session-log-<N>-<topic>.md`, N = highest existing + 1, using `assets/session-log-template.md`. A decision made in conversation but not written down does not exist.
3. **Update the plan** per the `investigation-update-fields` allow-list: checkboxes (with resolution pointers), dive status, artifact links, header count.
4. **If the dive is now `Concluded`** (every question checked or relocated): check the §5 mapping and tell the human which deliverables just became startable — and whether every dive feeding one of them is now done.

## Anti-patterns

- Musing: a session with no artifact, no lock, and no relocation produced nothing — say so in the log rather than dressing it up.
- Treating a prior leaning as a lock because it was written down.
- Ending the session without the log because "it was mostly discussion" — discussion is exactly what the log preserves.
