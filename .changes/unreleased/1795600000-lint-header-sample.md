---
type: Changed
---

`make precheck` lints a changed header through one file that includes it instead of every includer (`LINT_SAMPLE=0` lints them all, and main still lints every file). `tools/lint.sh` now reads Ninja's dependencies when the build directory is inside the tree, where it had found no includer of a changed header at all.
