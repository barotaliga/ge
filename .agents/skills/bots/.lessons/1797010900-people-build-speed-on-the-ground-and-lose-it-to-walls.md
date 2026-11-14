---
title: People build speed on the ground and lose it to walls; a walker flies its own box, and a stop is read by its kind at the recording's rate
date: 2026-11-10
scope: plugins/shooter/pilot.cpp, plugins/shooter/look_ahead.cpp, plugins/shooter/circle_jump.cpp, games/shooters_edge/labs/bumps_read.hpp, games/shooters_edge/labs/se_moves_lab.cpp, games/shooters_edge/labs/se_recipes_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [circle-jump, ground-strafe, hop-start, bumps, bump-cause, ahead, run-up, backstep, stops, uturn, frame-rate, se-moves, se-recipes, speed]
---

- Split alive time by pilot mode and speed first: dm2's experts walked at 280+ a third of their
  lives and hopped 6-7 %, so the stretches never started outweigh a flight's gain. Count why a
  stretch is refused; a stop rule another guard now covers can go.
- A chain's first jump leaves at 433 for people: built ON THE GROUND by a circle jump (the heading
  turning 51-75 degrees in 0.3 s, the view on the velocity, the wish 45-50 off it). Above 320 only a
  wish ~30 degrees off gains, so under a view not the circle's own every circle stalled at ~340.
  Split takeoffs by the ground before them (bots 53 % at 324, people 31 % at 391).
- Name what took the speed before fixing a bump (`team.bumps.by-*`): a third were bots into each
  other; only `wall` is the walk's. Pure pursuit never takes the old direction's speed off at a
  corner. `ahead` flies the keys through `player_move` for the hands' delay, then each candidate
  way, and presses the one that does not bump nearest the aim (wall bumps 12.7 -> 10.3).
- se-people `bumps.*` reads bumps off recordings alike: people 2.5 a minute at speed, bots 4.4-5.2.
  Per minute AT SPEED is the fair rate.
- Stops: equal at 0.1 s samples, 12.3 against 18.8 a minute at the recording's rate. Classed by the
  motion across (still, reversed, turned, on), the gap was a run reversed through zero: a turn back
  pressed across the run (`uturn`) gained 4.5 a match. Step back to a run-up facing the jump
  (`backstep`).
