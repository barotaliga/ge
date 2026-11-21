---
title: The log's item goal is not where the bot goes while a role holds it; an item score moves only free bots
date: 2026-11-21
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/fighter_items.cpp, games/shooters_edge/labs/chain_read.hpp
concepts: [item-goal, role-goal, roles, hold, push, rush, circuit, chain, round, high-mega, dm2, decision-log]
---

- `item_goal` is recorded where the fighter's own item choice runs, and the record stays the last
  one while the team's role steers: `team_goal` (`role_goal`: "into the base together", "on the
  area it claimed", the rush) goes first and the item goal is not walked. dm2: the high mega
  chosen 25 times in a match, reached once. Read `role_goal` beside `item_goal` before saying a bot
  went for an item.
- Hold, rush, push and carry are 70 % of a 2on2 bot's live time (`team.role.<role>.share`), so a
  change to `best_item`'s score reaches the free 30 % only: `circuit` (the next key item's worth
  added) moved the takes 1.71 -> 1.77 a minute. A round by the clock belongs in the roles.
- People's rounds are read off recordings by spot (se-people `chain.*` and its table): the red
  armour then its room's mega, the YA by the teleporter then the high mega. Ask that table which
  spots the bots under-take before building, not the item's lying time alone.
