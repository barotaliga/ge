---
title: An escape the pilot cannot fly as its proof did is no escape; stage it before a match
date: 2026-11-09
scope: plugins/shooter/escape.cpp, plugins/shooter/pilot.cpp, plugins/shooter/bot.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [escape, lava, rocket-jump, proof, pilot, hand, waterlevel, staged-run, dm2, no-route]
---

`no route` on dm2 was bots in LAVA: the nav closes lava and slime to entry, so a bot that falls
in has no link out. The rocket out (`shooter/escape.hpp`, feature `escape`) is proven by the
move, and every gap between the proof and the pilot showed up as a live failure:

- The pilot read the SERVER's waterlevel, the proof the move's (`water_level`): use the move's.
- The pilot pitched the view only on the firing command; the proof held it all the backing (in a
  liquid a pitched view turns the back key's wish UP). Share one keys function and use ALL of it.
- A proof pitched 88: the hand stops at 80, so the shot never came. Never prove a view a player
  cannot have.
- The hand needs ~0.3 s to stroke 80 down: the view goes down from the start of the backing, the
  takeoff 24 under the surface so the rise is the stroke's time.
- A shot from a swimmer sinking back loses half the lift; one from under the surface most of it.
- A new goal makes the pilot idle (`bot_goal`) mid-flight: a flight restores its mode and plans
  on landing; one that died in flight must NOT keep that guard (a bot idled all match).

Rare events cannot be judged in matches: STAGE THEM. `se-reach --escapes=fly` flies every escape
with the pilot (`run_route`) and tables fired / landed: 0 -> 18 -> 31 landed as each gap closed,
minutes a round, where a match showed one fall in four minutes. Deep lava burns ~120 health a
second, so most live shots come from bots nearly dead: read `team.escapes-out` against
`team.escapes`, and look at WHY bots fall in.
