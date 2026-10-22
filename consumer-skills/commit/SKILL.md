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
ge check                   # format, ge doctor, the build, every test and lab (debug: sanitizers)
ge precheck                # before the push: CI's gates in both presets (check, pilot, replays)
```

Each finding says what, where and the next action; a format finding's next is the
`clang-format -i` line to run (`make fmt` formats every file at once).

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

CI is the gate. After the push, check the PR every 15 minutes until it is merged: `ge ci <pr>`
says whether it still merges with its base (a conflict is an error finding: merge the base,
resolve, `ge precheck`, push) and reads each red job's log back as findings (fix the cause, `ge
precheck`, push). Never skip or weaken a test to get
green.
