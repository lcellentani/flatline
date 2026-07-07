---
model: haiku
---

Run `git diff --cached` to inspect the staged changes.

The delegation tier is: $ARGUMENTS
Valid values: T1, T2, T3, T4 — as defined in .claude/skills/git-commit.md.
If $ARGUMENTS is empty or not a valid tier, stop and ask: "Which delegation tier? (T1/T2/T3/T4)"

Generate a commit message following .claude/skills/git-commit.md.
If the diff spans unrelated concerns, propose how to split into separate commits first.

Output only the final message(s), ready to copy. No explanation.
