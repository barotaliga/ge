---
title: A still view loses head to head to a snapping one; give the calm view a glance on purpose
date: 2026-11-08
scope: plugins/shooter/bot.cpp, plugins/shooter/features.cpp, games/shooters_edge/net/classic_team.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp, tests/shooters_edge_fight_test.cpp
concepts: [calm-view, glance, scan, facing, in-view, unseen, head-to-head, trial, novice, preset, temperament]
---

The calm view (a hand on a mouse, still along the route) lost about 61 frags a six-seed set to a
side without it. se-team's per-side block (`--sides=1`) said why: with an enemy in plain view the
side without it had him on screen 74 % of the time against 67 %, and the misses were to the side
and behind. Without the calm view the view snaps to every turn of the route (164 degrees a second,
people 107): an accidental scan. Making the calm view follow the motion (people look within 15
degrees of it half the time; se-people `moving-view-off`) gained nothing over twelve seeds. A
glance on purpose did: every second or two, 75 or 120 degrees to the side whose line of sight runs
longest, then back, at a hand's pace (about 54 frags a set head to head, the turn rate still a
person's). Try a fix head to head with the `trial` switch (`--features-red=+trial`, off in every
preset), on twelve seeds. A behaviour test's claim can hang on it: the glance made cowardly novices
see trouble coming, and "aggressive novices frag more" swung by block of four seeds (12 seeds
250-230, 8 seeds 149-171). Do not pick the seed count that passes; ask whether that level of
player does the thing at all (a novice has tunnel vision: his preset has no glance).
