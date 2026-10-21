---
skill: commit
scope: gates
---
`make fmt` and `make fmt-check` read `git ls-files`, so a NEW file is invisible to both
until it is added: the local gate passes and CI's `fmt` job fails on it. Run
`git add -A` before `make fmt` (and before `make agents-check`, which reads the same list).
