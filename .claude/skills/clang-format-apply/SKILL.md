---
name: clang-format-apply
description: Apply the project's .clang-format style via `clang-format -i`. Use when asked to format or run clang-format, after writing/editing .cpp/.hpp/.cc/.hh/.cxx/.h files, or before committing C++ changes.
---

# clang-format-apply

Runs `clang-format -i` against the project's `.clang-format`
(BasedOnStyle: Google, 4-space indent, 100-col — see
`.claude/rules/coding_style.md`).

## Resolving the file set
- Explicit paths given → use exactly those.
- Otherwise, uncommitted changes exist → `git diff --name-only` +
  `git diff --cached --name-only`, filtered to C++ extensions.
- Otherwise (explicit whole-repo request only) → `src/`, `test_*.cpp`,
  `wizard_of_oz_demo.cpp`. Never `third_party/` or `build/`.

## Running
`clang-format -i <files>` in one invocation. If `clang-format` isn't on
PATH, say so and stop — don't guess a path.

## Reporting
clang-format is silent on success, so report via `git diff --stat` after
running: which files actually changed. Don't enumerate untouched files.

## What not to do
- Don't run on every micro-edit mid-task — batch at a natural checkpoint
  (end of turn, before a commit).
- Don't reformat files outside the current change scope.
