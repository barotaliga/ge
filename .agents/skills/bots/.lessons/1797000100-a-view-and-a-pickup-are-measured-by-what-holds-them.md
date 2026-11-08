---
title: Break a bot's view turns down by what holds the view, and its pickups by kind, before tuning either
date: 2026-11-08
scope: plugins/shooter/bot.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/knowledge.cpp, games/shooters_edge/labs/se_team_lab.cpp, tests/shooters_edge_fight_test.cpp
concepts: [view-turn, calm-view, swim, release, pickups, on-the-way, short-goal, temperament, coward, se-people, a-b]
---

The bots' view turned 162 degrees a second against people's 107 (`se-people` view-turn), and the
calm view was not the cause: sampled every 0.1 s by what held the view, the calm walk and the aim on
a seen enemy were already people-like (55 and 86), while the swim (549; wading 1074, the next cell
almost underfoot), the turn back after losing an enemy (164, the aim's hands at full speed) and the
watched spots (188) were not. A swim turned at a hand's pace with the way pressed by the keys under
the view, and the turn back at 360 a second handed to the calm view within its band, gave 103 and
7 % fast (people 6 %). Turning back at 180 cost 0.9 frags a minute: the bot held a lost enemy's spot
twice as long and saw less. Pickups by kind (`team.took-per-min.<kind>`, beside people's KTX numbers)
showed small health 0.5 a player-minute against people's 3.6. Short goals on the way (an item worth
having within 256 units ahead) first went for ammunition too and cost frags (detours, speed 164 ->
152); kept to health, armour, a missing weapon, the mega and rockets, and never while backing off
(a retreat picks its health away from the enemy), they gave frags 10.6 -> 12.1 and damage 184 -> 208.
A test claim about an outcome on four short seeds can stand on one death: "aggressive novices die
more than cowards" held 75 to 74 and was a coin toss over 12 seeds whatever the cowards did (cover
first: they died more, shot crossing open ground; fighting back nearer: they fought twice as much
and died as often). Measure the claim over 12 seeds before blaming the change; the user chose to
state what holds (they frag more).
