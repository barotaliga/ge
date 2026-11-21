---
title: A view eased back to its path turns back, and a reversal count needs the window round its edges
date: 2026-11-21
scope: plugins/shooter/pilot_recipes.cpp, plugins/shooter/bot.cpp, games/shooters_edge/labs/team_watch.cpp, games/shooters_edge/labs/se_recipes_lab.cpp
concepts: [reversals, view, ease, seam, hand-over, recipe, hop-start, startview, startkeep]
---

- An offset added to a played-back view that must be back to zero by the ease's end (a rate matched
  at the start, then gone) turns the view back by construction: its rate changes sign at a third of
  the ease. Carry a rate on and let it die out without returning (the path left turned by what was
  carried), or do not add one.
- A count of reversals over a window (se-recipes' starts) misses the ones at its edges: a writer
  handing the view over is a reversal neither writer makes. se-team's `view40.*.seam-share` counts
  those apart (23 % of all on dm2 2on2); read it before blaming one writer's own turning.
- A copied start's view must begin on its own line: begun at the bot's real view (a look's, the
  calm view's), its keys press the wrong way and the proof fails (`startview`, dropped). A person
  turns onto the way first.
- `restart()` drops a copy under way: `team.recipe.ended.<why>` names the caller. The fighter's new
  goal ended a fifth of them; keeping the copy through it moved no human measure (`startkeep`).
- A set started before a rebuild runs the old binary to its end (macOS keeps the image): rebuilding
  mid-set is safe for that set, and its numbers are the old code's.
