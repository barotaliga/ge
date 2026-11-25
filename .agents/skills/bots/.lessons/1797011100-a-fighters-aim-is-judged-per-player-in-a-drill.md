---
title: A fighter's aim is judged per player in a drill, its reaction over real sightings, and a trick by whether a person's eye is fooled
date: 2026-10-30
scope: plugins/shooterbots/senses.cpp, plugins/shooterbots/aim.cpp, plugins/shooterbots/dodge.cpp, plugins/shooterbots/fighter.cpp, games/shooters_edge/bots/duel_run.cpp, games/shooters_edge/net/qw_hits.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [senses, sight, aim, reaction-time, hit-rate, drill, trigger, view-off, target-switch, dodge, eye, selfaim, lead, accuracy, hearing]
---

- Players carry modelindex 0 until a program sets one: send them by the rule (in use, active, not
  spectating). A contact keeps `was_visible`, or lost sightings count twice. Round before
  `quantize_axis`.
- Time a reaction only over a sighting after a 0.3 s gap with the view off the target (a pre-aimed
  one by the trigger): a one-frame flicker at the cone's edge is tracking.
- Hit rates far under the bands were not the aim: an emptied respawn read as shots, the trigger
  compared a view a swim had replaced, players stacked by a teleport fired at a box. Each showed per
  player and in a drill (`se-team --drill=<weapon>`), never in the mixed table. Judge a projectile's
  lead in `--drill=rockets` first.
- People fire 37 % of plain-view time, bots 23 %: split view-off by how far and what turned it; past
  30 degrees was SWIMMING. A wide view (120 x 88.5) cost frags until the target stuck to the one in
  hand unless another is half as far: each new nearest drew a fresh reaction.
- The dodge lost because the aim read a velocity's turn the instant it happened, and the dodger's
  aim ignored his own strafe: with the course read a reaction late (`eye`) and the view worked from
  the eye ahead (`selfaim`) it went neutral, accuracy 31.7 -> 35.9. Ask whether the opponent model
  can be fooled at all.
- A behaviour on "a sound while calm" ran 5 times a minute across eight bots: there are no
  footsteps; most sounds arrive mid-fight.
