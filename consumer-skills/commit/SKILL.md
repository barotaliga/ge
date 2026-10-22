---
name: commit
description: "Use to end every work session in a game repo: the gates (format, agents-check, tests in both presets), the public-repo check, a conventional commit, the push, and the PR with its evidence."
---

# Commit, push, PR

## 1. Reflect

Run the `skill-reflection` skill's CLOSE pass for every skill the session loaded, so the lessons
ship in the same commit as the work.

## 2. Gates (cheapest first)

```sh
make fmt                 # then `git diff` to see what it changed
make agents-check        # ge doctor: every finding says what, where and next
make test                # debug, with the sanitizers
make test PRESET=release
```

All green. A failure you did not cause is still yours to fix or report.

## 3. Public-repo check

Read `git diff --cached` once with one question: is anything here a secret, a personal detail, or
something you would not publish? No tokens, no e-mail addresses, no home paths, no internal
notes. Credentials belong in the CI's secrets.

## 4. Commit

A conventional subject scoped by directory: `feat(sim): ...`, `fix(labs): ...`, `docs: ...`,
`ci: ...`, `test: ...`. The body says what changed and why.

## 5. PR

The title is a conventional subject. The body holds:

- **What**: the change, in a few lines.
- **Evidence**: the tests added, and every lab of the subject before and after
  (`--tag=before`, `--compare=before`; skill: `labs`), with the pictures that moved. A digest
  that moved on purpose says why.

CI is the gate: read a red job's log, fix the cause and push. Never skip or weaken a test to get
green.
