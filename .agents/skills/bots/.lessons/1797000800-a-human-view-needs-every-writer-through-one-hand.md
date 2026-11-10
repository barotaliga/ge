---
title: A human view needs every writer of the view through one hand, and a measure by path
date: 2026-11-09
scope: engine/ai/mouse.cpp, plugins/shooter/bot.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [mouse, hand, view, jitter, reversals, sweep, tilt, lift, finger, click, shot-timing, se-people, view-by, override]
---

The bots' view looked like a bot's (level lines, jitter). `se-people`'s `hand.*` measures read
people's and bots' recordings alike: sweeps held dead level 3 % for people against 60 % for the
bots, the pitch frozen while turning 2.5 % against 45 %, reversals 0.09 against 0.74 a second.
The mouse hand (`engine/ai/mouse.hpp`: minimum-jerk strokes at Fitts' time, the pad's reach
and lifts, the arc) on the aim and the calm view moved them only to 52 % and 0.40: most of the
view was written by OTHER paths. The pilot's own heading in jumps, hops and swims, and the
fighter's after-the-fact views (a button shot, a doorway fired at, the swim's way out) each set
the angles in one command. Tag each command with what turned the view (`Bot::view_by`, se-team
`--trace` columns `yaw pitch view`, the private `view.py`) and count reversals by path before
tuning the hand: the hand's own paths read 0.05 a second, the rest were the overrides. Route
every writer through the one hand (`bot_hand_override` for a view set after the step), with the
wish turned under the hand's view (`turn_axes`, `turn_axes_swim` under water) so the move is
unchanged.

People's tilt is small: a right hand's drag right runs down 0.008 degrees a degree (and up
going left), not the 0.04 a geometric guess gave; most of their pitch change in a sweep is
scatter (0.09 degrees a degree, either way). Measure the yardstick before choosing a default.

A finger that clicks only while the view is ON the target fires late (a click's gap after the
weapon is ready): head to head it cost about 100 frags a six-seed set. Clicking while roughly on
him (2.5 times the trigger's tolerance) and timing a share of the clicks to the refire (`sync`)
halved that.
