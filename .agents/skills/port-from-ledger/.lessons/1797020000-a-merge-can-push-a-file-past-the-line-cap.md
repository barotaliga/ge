---
title: A clean merge can push a busy file past the line cap
date: 2026-11-22
scope: games/shooters_edge/client/main.cpp
concepts: [line-cap, merge-queue, agents-check, main-cpp]
---

The window's `main.cpp` sits a few lines under the 2000-line cap, and every thread adds a line or
two there. A PR that passed `make agents-check` on its own branch was ejected from the merge queue
because main had grown by a line meanwhile and the merged file came to 2001. Before arming
auto-merge after merging `origin/main`, run `tools/check-agents.sh` on the merged tree, and keep new
wiring out of `main.cpp`: register a command from the subject's own file (`add_path_command` in
`client/game_folder.cpp`) and fold related assignments into a helper (`third_person_eyes`).
