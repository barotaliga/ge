---
title: Split a trigger that holds fire by how far off the view is and what turned it, before tuning the aim
date: 2026-11-08
scope: games/shooters_edge/bots/fighter.cpp, plugins/shooter/senses.cpp, plugins/shooter/bot.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [trigger, view-off, swim, fire-in-view, tolerance, fov, target-switch, reaction, se-people, se-team, a-b]
---

People fire 37 % of the time an enemy is in plain view, the bots 23 %, and `team.trigger.view-off`
said 35 % of the trigger's looks found the view off him. A temporary histogram of how far off (within
twice the tolerance, to 30 degrees, past 30) and whether the command's view was still the aim's
split that in two: the near misses were tracking sway, but most of the misses past 30 degrees came
while SWIMMING, where the swim had taken the view to steer, so a bot in water never shot back. With
the view kept on him and the swim's way pressed with forward, side and up under it, view-off went
35 -> 31 % and damage 171 -> 177 a player-minute (six seeds). A screen-shaped view as wide as
people play (120 by 88.5 degrees) saw more (64 -> 69 % of in-view time) but cost about 0.5 frags a
minute in three pairings: the target was the nearest enemy in sight every frame, so each one a wide
view brought in nearer took the aim and drew a fresh reaction. Kept on the one in hand unless
another is half as far, the wide view gained instead (frags 10.5 -> 10.9, damage 177 -> 189,
reactions 1020 -> 913 a match): a change that should help and hurts is read for what it newly
triggers. Keeping the trigger down through a sway of twice
the tolerance moved `se-people`'s fire-in-view by only 2.5 points, not more at three or five times,
and its extra fighting broke a pickup floor in the arena test. The gap left is facing (an enemy in
the bots' view 68 % of in-view time against people's 84 %, by the same 120-degree screen), not the finger. `se-people` on the bots'
own recordings (`se-team --save=x.mvd`, then `se-people --file=x.mvd`) is the yardstick; the
trigger's own shares only tell where to look.
