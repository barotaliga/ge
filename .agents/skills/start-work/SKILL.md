---
name: start-work
description: "Use before beginning any task in gameengine2, before edits. Confirms a clean tree, syncs with origin/main, clones the read-only oracle repos beside the checkout, and names the docs/goals.md milestone row the task serves."
---

# Start work

Run this preflight before you start the task. You may read files to find your way
first, but make no edits until the preflight passes.

## 1. The tree

```sh
git status --short --branch
git branch --show-current
```

- A dirty tree is the user's work until you prove otherwise. Never reset, stash or
  delete it to get a clean tree. If you cannot tell who owns it, ask.
- If a merge or rebase is in progress, finish or abort it before anything else.

## 2. Sync

```sh
git fetch origin main
git rebase origin/main          # on a feature branch; on main: git merge --ff-only origin/main
```

If the branch's PR is already merged, restart the branch from `origin/main` and do not
stack new commits on merged history.

## 3. The oracle

```sh
[ -d ../game5 ] || git clone --depth 1 https://github.com/niclaslindstedt/game5 ../game5
[ -d ../oss-game-framework ] || git clone --depth 1 https://github.com/niclaslindstedt/oss-game-framework ../oss-game-framework
```

Both are **read-only**. Never commit, push or comment there.

## 4. The row

Open `docs/goals.md` and name the milestone row this task serves. If no row fits, the
task is probably out of scope (G4), so ask before you build it. Then load the skill
`AGENTS.md` routes the subject to, plus `write-code`.

## 5. The build works before you touch it

```sh
make test
```

If it is red before your change, that red is the first thing to fix or report.
