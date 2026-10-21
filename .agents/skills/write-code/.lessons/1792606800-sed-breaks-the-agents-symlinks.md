---
title: A sed -i sweep over git ls-files turns the AGENTS.md symlinks into copies
date: 2026-10-21
scope: AGENTS.md, CLAUDE.md, GEMINI.md, .cursorrules, .github/copilot-instructions.md
concepts: [symlinks, sed, sweep, agents-check]
---

`git ls-files` lists `CLAUDE.md`, `GEMINI.md`, `.cursorrules` and
`.github/copilot-instructions.md`, and `sed -i` on a symlink writes a regular file in its place,
so a tree-wide rename leaves four stale copies of AGENTS.md that `make agents-check` then
rejects. Exclude symlinks from a sweep (`git ls-files -s | awk '$1 != "120000"'`), or restore
them afterwards with `rm <link> && git checkout -- <link>`.
