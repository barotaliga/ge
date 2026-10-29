---
name: commit
description: "Use to end every gameengine2 work session: run the gates, write a conventional commit, push the branch, open the PR with its milestone row and evidence, drive CI to green, and squash-merge once green. Also for anything about the merge queue: arming, holding PRs behind one (`queue-priority`), merging without CI (`skip-ci`), ejections and re-arms, stale runs, and a red test that may be a flake."
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

`make test` includes the `labs_smoke_*` slices: every lab, twice, against `tests/digests/`.

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
auto-merge on the PR: `main` merges only through GitHub's merge queue, and auto-merge is how a
PR joins it (`gh pr merge <pr> --auto --squash`; an agent tool that only "enables auto-merge"
may not enqueue on a queue base, so read the PR's state back after arming). CI runs no Linux
build, test or lint: `make precheck` is that gate, so a push without it lands unchecked on Linux.
A pull request runs the gates, Windows and macOS; the queue runs them again on the tree that
lands, with the packaging, and a job only the queue ran ejects the PR with that job's log: fix
it, push, and it queues again. Label a PR `full-ci` to add the Linux rows, the lint and the whole
packaging before queueing a risky change (a compiler-sensitive one, a build change). A direct merge is
refused, so `ge ci <pr> --merge` on a queue base arms auto-merge instead and says so. `ge ci <pr> --wait`, run in
the background, is the check-in as one call: it returns when a check fails, the PR conflicts,
or every check is green. Then
name the next step to the user.

**The queue's hands** (`.github/workflows/queue.yml`): nobody disarms, messages and re-arms other
threads' PRs by hand. To let one PR reach the queue before the rest (a structural change every
other branch must merge), label it `queue-priority`: every other PR that is armed but not yet
queued is disarmed and labelled `queued-hold`, with one comment, and re-armed by itself when no
open PR carries the label any more (it merged, closed or lost the label). Leave a `queued-hold`
PR armed-off; keep it clean against main. A queued run that ended with no failed job (cancelled
or abandoned jobs only) re-arms the PRs it ejected by itself, with a comment naming the run; one
with a real failure is yours to fix. Never cancel a `merge_group` run to free runners: it ejects
every entry queued behind it.

**`skip-ci`**: a PR labelled `skip-ci` runs no CI at all, on the PR and in the queue, so it merges
as soon as it is armed. Only Niclas decides which PRs get it. Whatever it breaks on main is fixed
afterwards by its owner, in the next PR, before any other work on that thread.

**A red test that is not yours** (`.github/workflows/flakes.yml`): every failed run's failing tests
go into the `ci-ledger` issue, and a test that failed on two different PRs within a day gets a
`Flake: <test>` issue labelled `flake`, fed with every later sighting. Before you chase a red test
in your PR, look for its flake issue; if there is one, say so on your PR and rerun the failed job
once. The flake itself is fixed by whoever takes the issue, never skipped or weakened.
