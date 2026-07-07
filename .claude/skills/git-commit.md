---
name: git-commit
description: "Reference for the commit message format: [Tn] type(scope): subject. Not user-invocable directly — used internally by the /git-commit command."
user-invocable: false
---
  
# Commit format

`[Tn] type(scope): subject`

- `[Tn]` is the delegation tier prefix: [T1], [T2], [T3], or [T4]
- `type(scope)` follows Conventional Commits
- `Subject` uses imperative mood, ≤72 chars, no trailing period

# Example

[T1] feat(ecs): add sparse set component storage

Replaces the naive vector<any> prototype with a proper sparse set.
Lookup is O(1), iteration is cache-friendly, removal is O(1) swap-erase.

Delegation: T1
