---
title: Measure where an item trip fails before choosing who goes, and judge facing by the enemy appearing
date: 2026-11-13
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/bots/buttons.cpp, plugins/shooter/pilot.cpp, plugins/shooter/bot.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [item-control, armour-taker, fetch, button, poke, scripted-way, script-step, facing, appear, calm-view, h2h-noise]
---

- A team taker for the YA (`fetch`) cut its lying 83 -> 72 % in a six-seed set and LOST head to
  head (-4.5): the trips failed on the way (a climb, the pool, a fight), not for want of a taker.
  Before building who goes, split the trip's time by place and pilot mode off the trace
  (`--client-fed=0` gives `goal_real`): the faults it finds (a stale script step, a button
  circled) are the cheaper fix.
- A six-seed, five-minute set swings frags a minute by +-30 % (`pokestand` read +2.8 there and
  -0.2 head to head; the sides of one head to head differ by up to 14 frags). Read a small set
  for the behaviour's own measure, never for frags.
- State kept per link (`script_step`) must be reset when the pilot comes back to the link, not
  only when the link changes: a bot that left the YA's stairs and returned resumed at the top
  step and walked into the riser.
- A button a shot opens is never pressed by touch: the poke's "walk into it" ran bots round
  under dm2's RA button at 300 units/s, and the mouse hand (which holds the trigger until the
  view is on the target) never fired.
- FACING: "an enemy in view" counts enemies anywhere round in a clear line; the sharp measure is
  the enemy APPEARING (newly in a clear line) and whether he is on the screen then, split by
  where he lies against the run. People and bots meet enemies from the same sides; people have
  the ones ahead of their run on screen 90 % (bots 69 %). Three calm-view targets (the way in
  sight, the velocity, the walk with a tight band) all did worse: the run turns faster than a
  paced view follows.
- Never wait on a job with `while pgrep -f "<x>"` from a shell whose own command line contains
  `<x>`: it matches itself and never ends. Wait on the job's output file, or its exact binary.
