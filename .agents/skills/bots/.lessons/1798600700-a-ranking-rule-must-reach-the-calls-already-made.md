---
title: A new rule for who goes must also reach the calls already made and the roles already held
date: 2026-11-21
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/bots/buttons.cpp, games/shooters_edge/labs/show_read.hpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [pokelook, view-writer, poke, look, reversals, rushstrong, rush, rushhold, coming, quad, strong, showfight, fight-at-showing, ranking, team-totals]
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
- Two writers of the view in one command fight through the one hand: a view set after the step
  (`bot_hand_override`) strokes the hand the step's own writer just stroked the other way, and the
  mouse hand's planned strokes swing between them (the poke under dm2's shot button: 109 -> -166
  degrees every 0.2-0.3 s). The trace's `view` names only the LAST writer, so it read "poke"
  throughout. A spot to keep the view on is the bot's `look`, set before the step: one writer
  (`pokelook`: reversals off the recordings 0.54 -> 0.34 a second, people 0.12).
