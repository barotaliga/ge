---
type: Added
title: ge goals free
---

`ge goals free [<row>]` reads the roadmap (`docs/goals.md`'s rows and `docs/goals/M*.md`) at the base, at every remote branch and in the working tree, lists each milestone row only a branch holds with the branch and the row's name, and names the next free number, so two pull requests are never given the same new row. With a row (`ge goals free M17`) it does the same for the letters under it. `--fetch` fetches every branch first; `--base` names another base.
