---
title: A new rule for who goes must also reach the calls already made and the roles already held
date: 2026-11-21
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/labs/show_read.hpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [rushstrong, rush, rushhold, coming, quad, strong, showfight, fight-at-showing, ranking, team-totals]
---

- `rushstrong` first ranked the quad's rushers by strength in the distance test only: 124 thinks a
  match changed their ranking and the rushes begun strong moved 17.6 -> 18.9 %. Two older rules
  outranked it: a mate who had SAID "coming quad" counted ahead whatever his strength, and a rush
  once begun is held 25 s (`rushhold`). The weak bot that thought first kept the rush. With the
  strong bot outranking a weak mate's call past the edge and the weak rusher's hold yielding to a
  strong mate's said one: 20.9 %, the quad's showings with a strong player near 19 -> 25 %.
  Before judging a rule for who goes, list every path by which someone already counts as going.
- Read the yardstick off the outcome (who stands at the showing, se-people `showfight.*`), not the
  rule's own counter: a counter of ranking changes can be large while the outcome barely moves.
- A losing fight at an item was not the bots' fault: people's weak players lose there as often
  (the launcher's duels 90 %, the better armour's 92 %). The gap was who stands there strong.
  Split a bad outcome by the same measure in people's recordings before fixing the loser.
- se-team reads `net::TeamTotals`, copied field by field from `bots::TeamStats` in
  `qw_team.cpp`: a new counter (and an old one, `rushes`, which read 0 for months) needs its copy
  there, or its metric is silently zero.
