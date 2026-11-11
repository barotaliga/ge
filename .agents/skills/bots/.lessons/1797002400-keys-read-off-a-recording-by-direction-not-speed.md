---
title: People's keys are read off a recording by direction, not speed, and a pilot that flies analog keys breaks when a keyboard presses them
date: 2026-11-11
scope: games/shooters_edge/labs/keys_read.hpp, games/shooters_edge/labs/se_people_lab.cpp, engine/ai/keys.hpp, games/shooters_edge/bots/keyboard.cpp, plugins/shooter/pilot.cpp
concepts: [keys, keyboard, mvd, inferred-keys, octant, feathering, run-up, look-ahead, brake, ground-strafe, se-people]
---

- A multiview recording updates a player ~29 times a second and each update holds 2 or 3 of his
  13 ms commands, so a frame's speed is off by up to a third (an acceleration read off three
  frames is noise: 25 key changes a second) while its direction is exact. Read keys by
  direction over long windows (`keys_read.hpp`): a steady run's heading off a STEADY view (the
  velocity lags a turn by ~0.25 s; under 3 degrees over 8 frames), only at a run's speed
  (280-340: past it is a hop's speed sliding on the floor), the air's curve for the side key.
  Check a reader on a histogram first: people's headings peak sharply at 0 and +-90 off the
  view; a median off the eight sets of 11.25 degrees means the read is random.
- Putting keys under a pilot tuned on analog axes broke it where it controls speed or picks a
  way by flying analog keys: a run at a jump holds the jump's proven speed with forward part-way
  (a full key overran its window: run-ups 6 -> 24 % of the time, flights 5 -> 0.2 %, fixed by
  feathering the key round the speed wished); a walk to a near point orbited it at full speed
  (the set held 10 degrees past its edge: cut to 4) and stopped short of a run-up point (a wish
  under the press threshold: on the ground any wish is feathered); and a key set 20-45 degrees
  off the velocity pushes the body past 320 on the ground (the ground strafe), while the
  look-ahead's brake proved bump-free as an analog side share becomes a full diagonal. Wall bumps
  doubled. The pilot must fly the keys it will press (the user's rule: a bot knows how its body
  meets the room). A view moved so the walk lies on a key set changed nothing: trace before
  steering by the view.
