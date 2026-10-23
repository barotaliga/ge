---
name: commit
description: "Use to end every gameengine2 work session: run the gates, write a conventional commit, push the branch, open the PR with its milestone row and evidence, drive CI to green, and squash-merge once green."
---

# Commit, push, PR, merge

## 1. Gates (cheapest first)

```sh
make fmt            # then: git diff, to see what it changed
make agents-check   # structure, ge: tags (writes docs/index.md), lesson fragments
make test           # debug preset, with ASan + UBSan
make test PRESET=release
make precheck       # last, before every push: CI's cheap gates as CI runs them (about 40 s warm)
```

`make precheck` (`tools/precheck.sh`) stops at the first red gate: fmt-check, agents-check,
libm-check, the fragments, the build, lint over what the change reaches since `origin/main`,
and the tests it reaches (`cli_diagnostics_test`, each changed test, each changed file's
`ge:test`). Those are the gates that cost the most CI rounds; run it after the last edit, every
push.

`make test` includes the `labs_smoke_*` slices: every lab, twice, against `tests/labs_digests.txt`.

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

- **Milestone**: the `docs/goals.md` row, and the change made to its status file (`docs/goals/<row>.md`) in this PR.
- **What**: the change, in a few lines.
- **Evidence**: the golden tests added (rows × functions, all bit-exact), and the labs of
  the subject run before and after (the `--compare` table and the captioned pictures that
  moved; the `labs` skill). If a lab's digest moved on purpose, say which and why.
- **Next step**: the next open row.

## 5. Green, then merge

CI is the gate. **After the push, check the PR every 15 minutes until it is merged**: schedule
the check-in (never a sleep loop), and on each one look at both its checks and whether it still
merges (`build/<preset>/ge ci <pr>` answers both: a conflict is an error finding, each red job's
log comes back as findings). A conflict means merge `origin/main` into the branch, resolve it,
`make precheck` and push; a red row means read its log, fix the cause and push. Never skip or
weaken a test, and never push an empty commit to re-run CI. Right after each push, arm squash
auto-merge on the PR (GitHub's `enable_pr_auto_merge`; it needs the base to require checks, and
GitHub refuses it on a PR that could merge now). Once every job is green and it has not merged
itself, `build/<preset>/ge ci <pr> --merge` squash-merges it at the head the checks ran on. Then
name the next step to the user.
