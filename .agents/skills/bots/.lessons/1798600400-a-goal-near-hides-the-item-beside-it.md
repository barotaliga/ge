---
title: A goal near hides the item beside it; ask the log why a key item close by was passed
date: 2026-11-20
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/fighter_items.cpp
concepts: [on-the-way, short-goal, item-passed, nearwait, mega, red-armour, dm2, holder, decision-log]
---

- dm2's south mega lies 96 units from the red armour. A bot whose goal was the armour (going for
  it, or standing on it waiting) never asked for a short goal: `on_the_way` runs only when the
  goal is over 256 away. The mega lay 60 s after it showed (people 8). `nearwait`: a goal that is
  not there leaves the short goals open (mega 60 -> 32 s).
- `item_passed` (the decision log) names, for each key item there within 256 and not taken, the
  first thing barring it, and whether the bot knew it was back (the record reads the server's
  model, decides nothing). Read its reasons before guessing at a pass; "another floor" is mostly
  bots through a wall, not on the item's floor.
- The log writes a row only when a choice changes, so reasons counted off it are changes, not
  time; a reason's time share needs a counter in the lab.
