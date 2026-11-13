---
title: A classic match ends at its limit, not when the map changes
date: 2026-11-13
scope: games/shooters_edge/net/classic_report.cpp, games/shooters_edge/net/classic_game.cpp
concepts: [match-report, match-tally, intermission, timelimit, fraglimit]
---

The classic rules ask for the next map only after their intermission, which holds the scores up
for several seconds. A report built at the map change would add the intermission to the match's
duration and to every player's seconds. So the tally keeps the last frame any frags changed, and
the end is the time limit's moment (the rules count it from the map's start) or that frame when a
player reached the frag limit.

Read the rules' limits as numbers (`Cvar::number`), never with `atoi`: a timelimit of `0.34`
minutes is a common lab and test setting, and `atoi` reads it as no limit at all.

The rules keep only the frags. Deaths, suicides and team kills come from watching the world each
frame: health falling to nothing is a death, frags falling on that frame is a suicide, frags
falling while alive is a team kill.
