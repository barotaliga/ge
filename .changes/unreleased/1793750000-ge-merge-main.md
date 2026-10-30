---
type: Added
title: ge merge-main settles the mechanical conflicts of a merge from main
---

`ge merge-main` merges `origin/main` (fetched first; `--base` for another ref) into the current branch. It settles the conflicts that have one answer: a file main deleted that this branch changed takes main's deletion, with a warning naming what to carry, and a lab's digest file both sides moved takes main's side, after which the lab is rerun and its file rewritten. Every other conflict is named and left in the tree, and the merge is committed once none is left. It refuses a tree with uncommitted changes or a merge already in progress, and names the precheck as the next step.
