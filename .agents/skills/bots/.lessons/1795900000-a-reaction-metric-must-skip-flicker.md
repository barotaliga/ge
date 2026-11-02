---
title: Time a bot's reaction only over real sightings: a flicker at the cone's edge is tracking
date: 2026-11-02
scope: plugins/shooter/bot.cpp, games/shooters_edge/bots/duel_run.cpp, tests/shooter_senses_test.cpp
concepts: [reaction-time, sighting, pre-aim, tracking-lag, duel]
---

A fixed hands delay doubles as the reaction and the tracking lag; a reaction drawn per sighting
needs the two apart, the delay cut to the lag and a freeze for the rest. The duel's reaction
metric then broke: it timed every frame the runner came back into sight, and most of those are
one-frame flickers at the cone's edge with the view still on him, timed near zero. Time only a
sighting after a gap (0.3 s, the same rule the bot draws a reaction by) with the view off the
target; a pre-aimed one is judged by the trigger (`bot_reacting`), not by the view. And a run
can time none: check the median when there is one and the count over all runs.
