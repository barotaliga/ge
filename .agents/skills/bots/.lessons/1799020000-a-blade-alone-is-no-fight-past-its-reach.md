---
title: A blade alone is no fight past its reach, and a stuck spot outlives the fix of its symptom
date: 2026-11-26
scope: plugins/shooterbots/fighter.cpp, plugins/shooterbots/bot_arsenal.cpp, tests/shooterbots_bot_arsenal_test.cpp
concepts: [outreach, blade, axe, out-of-ammo, retreat, loaded-reach, campaign, e1m4]
---

- An aggressive bot takes every fight (`takes_fight`), so out of ammunition it fought a monster
  across a pit with the axe for minutes. `loaded_reach` under 200 with the foe in sight past it for
  5 s now counts as outmatched: retreat, a weapon first (`out_reach` in the fighter's stats).
- On e1m4 seeds 2 and 4 the bot still spent 7 minutes in the north room (768, -300, 960) after
  that: its routes never advanced there in any mode (k stuck at 0 to 2, plan after plan), so the
  axe fight was a symptom of the room, not the cause. Taking the aim off the foe and holding the
  timer through a lost sighting changed nothing there and cost the exits step one gained.
- Count a stall by where the bot is and its pilot's `k`, not by its fight mode, before blaming
  the fighter: a `k` that never passes 2 on long routes is a pilot or a nav problem.
