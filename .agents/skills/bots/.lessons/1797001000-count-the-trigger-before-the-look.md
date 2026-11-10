---
title: Count how often a view behaviour can fire before you A/B it, and judge a watch by what it sees
date: 2026-11-09
scope: plugins/shooter/bot.cpp, games/shooters_edge/bots/watch.cpp, games/shooters_edge/bots/ears.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [listen, sound-swipe, glance, watch, frontier, hearing, sighting, in-view-seen, a-b, trace]
---

The sound's swipe (`listen`: a calm bot turns its view toward an enemy it hears but did not know
was there) ran 5 times a minute across eight bots on e1m2. The cause is the game, not a bug. There
are no footsteps. A player makes sound only when he jumps, fires, takes an item, cries out or rides
a mover. A bot's ears take in about 10 to 40 sounds a minute, and most of them reach it while it is
already fighting or in the air. So a behaviour gated on "a sound while calm" cannot move a match's
numbers. Count its trigger first: a stat in `BotStats`, a `se-team` count (`team.sound-swipes`),
or a temporary counter over a one-minute match. Then decide whether the A/B means anything.

The time a bot stands still is in the trace (`se-team --trace`: pilot `arrived`, `wait`, `ride` at
low speed, about 13 % of its time). It is not in `team.watch-share` (3 %): the watch runs only in a
planned area with ways in. Filling the rest with a generic watch (the nearest nav nodes in sight
that link to one out of sight) raised the time spent watching but lowered the share of enemies
seen (59.7 -> 56.7 %) and lost head to head. Those nodes are the corners beside the bot, not the
doorways people watch. Judge a watch by `team.side.<side>.in-view.seen` and `pre-aimed`, not by
the time spent watching.
