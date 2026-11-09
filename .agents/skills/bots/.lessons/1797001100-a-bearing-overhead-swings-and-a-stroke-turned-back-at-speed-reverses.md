---
title: A bearing to a point overhead swings, and a stroke turned back at 70 % still reverses the view
date: 2026-11-09
scope: plugins/shooter/bot.cpp, engine/ai/mouse.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [mouse, hand, view, jitter, reversals, swim, swimway, override, set-by, hop, sweep, turn-back, minimum-jerk, se-people, hop-share]
---

Most of the view's jitter outside the hand's own paths came from one override: the swim's way
(`fighter.cpp`), which pointed the view at the next swim cell every command. Name each override
(`Bot::set_by`, the trace's `view` column) before you tune anything. Here 5.5 % of the time
reversed 1.23 times a second. The cell lay nearly straight overhead, as when the bot rises to the
surface or rides the water lift. The bearing to a point overhead swings 40 degrees with every unit
of drift. A tolerance band on the override made it worse. The fix was to stop turning toward a
point nearer than 48 units across: swim to it under the view the hand already has, as people do
when they rise holding jump.

A minimum-jerk stroke still runs at 1.3 times its mean speed at 70 % of its time (v = 30 t^2
(1 - t)^2), and at a quarter of its mean at 90 %. So a hand that may plan a stroke the other way
at 70 % reverses the view at speed. People never show this at 40 Hz. The bots' hops did it at
every side switch. `MouseWant::turn_back` holds the rule.

Measure a behaviour on its own stretch of time. se-people's `hop.*` is the hand while the player
moves faster than 340 u/s across, which only a hop or a blast gives. It showed sweeps of the same
size as people's (79 degrees) with five times their reversals. It also showed that people hop 36 %
of their time alive on e1m2 and the bots 8 %. That one number explains most of the speed gap.
