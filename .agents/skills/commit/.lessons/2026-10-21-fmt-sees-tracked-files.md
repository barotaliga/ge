---
skill: commit
scope: gates (fmt, lint)
---
`make fmt` and `make fmt-check` read `git ls-files`, so a NEW file is invisible to both
until it is added: the local gate passes and CI's `fmt` job fails on it. Run
`git add -A` before `make fmt` (and before `make agents-check`, which reads the same list).

CI's `lint` runs a newer clang-tidy than Ubuntu 24.04's (18), and its
`clang-analyzer-*` set includes `security.FloatLoopCounter`, which 18 leaves off. A
local `make lint` pass is not proof: a floating-point `for` counter (common when porting
a JS scan that accumulates `h += STEP`) fails CI. Write such scans as `while` loops.
