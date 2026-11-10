---
title: A change to how bots fight moves everyone's hit rate more than the score: read accuracy over three seeds, and A/B a trait by team
date: 2026-11-06
scope: plugins/shooter/dodge.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/watch.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [dodge, watch, pre-aim, accuracy, a-b, traits, se-team, weapon-choice, held-share]
---

The dodge cut every bot's hit rate on e1m2 from 30 % to 27 % on all three seeds, but the frags
moved by less than the seeds did (88, 94, 87 before; 88, 77, 84 after). A team-wide change shows
first in `team.accuracy`; frags are the noisiest number a team match has. To show that a behaviour
helps the one who has it, give it to one team only: a trait per player (`--traits=dodge=8,...` red,
`dodge=1,...` blue) gives an A/B inside one match, which is why the dodge is a trait and not a
constant. A temporary `constexpr bool` off/on gives the before and after from one checkout; take
it out before the commit. Read the shares too: the dodge ran in about half the fight's commands
(the rest out of sight or on a proven jump), and the watch in only 3 to 5 % of the match because
bots seldom stand; a behaviour that rarely runs cannot move a number, so count how often it runs
before judging it. `team.held.<weapon>` showed the starting gun held 59 % of the match: the mode's
weapons go when picked up (`deathmatch 1`), so check the server's rules before calling that a
fault.
