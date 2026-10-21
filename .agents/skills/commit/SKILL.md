---
name: commit
description: "Use to end every gameengine2 work session: run the gates, write a conventional commit, push the branch, open the PR with its milestone row and evidence, drive CI to green, and squash-merge once green."
---

# Commit, push, PR, merge

## 1. Gates (cheapest first)

```sh
make index          # docs/index.md from the ge: tags (the `write-code` skill)
make fmt            # then: git diff, to see what it changed
make agents-check   # structure, ge: tags, index fresh, lesson fragments
make test           # debug preset, with ASan + UBSan
make test PRESET=release
```

`make test` includes `labs_smoke`: every lab, twice, against `tests/labs_digests.txt`.

All of them must be green. A failure you did not cause is still yours to fix or report.
Before the gates, run the `skill-reflection` skill's CLOSE pass for every skill the session
loaded, so its lessons ship in this commit.

## 2. Public-repo check

Read `git diff --cached` once with one question: is anything here a secret, a personal
detail, or something you would not publish? No tokens, no e-mail addresses, no home
paths, no internal notes. Credentials belong only in GitHub secrets.

## 3. Commit

A conventional subject scoped by directory: `feat(core): port the noise module`,
`fix(render): …`, `docs: …`, `ci: …`, `test: …`. The body says what changed and why,
and which milestone row it serves.

## 4. PR

Push with `git push -u origin <branch>`. The PR title is a conventional subject (PRs are
squash-merged). The body contains:

- **Milestone**: the `docs/goals.md` row, and the Status change made in this PR.
- **What**: the change, in a few lines.
- **Evidence**: the golden tests added (rows × functions, all bit-exact), and the labs of
  the subject run before and after (the `--compare` table and the captioned pictures that
  moved; the `labs` skill). If a lab's digest moved on purpose, say which and why.
- **Next step**: the next open row.

## 5. Green, then merge

CI is the gate. On a red row, read its log, fix the cause and push. Never skip or
weaken a test, and never push an empty commit to re-run CI. Once every job is green,
squash-merge the PR yourself, then name the next step to the user.
