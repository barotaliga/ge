---
title: After git mv, git checkout restores the pre-edit copy, not the moved and rewritten one
date: 2026-10-21
scope: plugins/, tools/check-plugins.mjs
concepts: [move, git-mv, checkout, sed, revert]
---

`git mv` stages the file as it was, so the include and namespace `sed` that follows lives only
in the working tree. Reverting a scratch edit with `git checkout <file>` then silently throws
the sed away too (old includes, old `ge:skill`, old `ge:test`). Revert scratch edits with
`sed -i '$d'` or an editor, or `git add` the rewritten files before experimenting; and after a
move, run `make agents-check` and the build once more before committing.
