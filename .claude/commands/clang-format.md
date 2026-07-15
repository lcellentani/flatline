---
model: haiku
---

Apply clang-format to: $ARGUMENTS

If $ARGUMENTS is empty, default to files changed relative to the working
tree (staged + unstaged) rather than the whole repository.

Follow the `clang-format-apply` skill for how to resolve the file set,
locate the binary, and report results.
