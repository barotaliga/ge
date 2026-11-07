---
title: A rocket jump's proof must start where the pilot can start it, and a strafed turn presses keys for the view when they land
date: 2026-11-07
scope: plugins/shooter/nav.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/pilot_run.cpp, tests/shooter_pilot_test.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [rocket-jump, flip, back-rocket-jump, strafe-turn, flip-keys, run-up, turn-room, bridge-pockets, hands, delay, proof, stepping-stone]
---

The flip (look back and down, fire with the jump, turn back in flight; `RocketJumpSpec::back`)
flies 45 % farther a second than the shot at the feet with a 40 % lower apex, and its pitch
places it. Getting the pilot to play what the bridge proved took these, each found in one trace
(`run_route` with the blast, `rocket_burst` and `rocket_blast`, no server needed):

- A proof that starts at 320 units/s against a wall, or with no floor behind it, is a link no
  pilot plays: it waits at the wall for ever, or circles a run-up point inside it. Refuse a
  running start that loses its speed on the first command, and ask the floor behind for the
  run-up (`room_to_run_up`), plus the turn's room for a flip (`turn_room`, a human's 0.4 s at 320).
- A refused proof left `end` at 0 0 0, which `lands_in` could take as a landing: a proof's `end`
  starts at its start.
- Tries in a loop over pairs (nearest first) with the techniques inside let the nearest pair pick
  the technique: put the technique outermost (running flips over every pair, then half speed,
  then standing, then the shot at the feet), or the skilled jump never wins.
- The approach takes over one grid link (32 units) before a jump; a human's turn round needs
  about 110 at full speed. Look ahead along the route (`flip_ahead`) and start turning from the
  walk. Lead the turn by the view's hand (delay + 180 / slew, within 0.02 s of measured), and by
  the link's speed, not the current one (the turn slows the run and the condition flickers).
- Keys for the view as it is now land 0.18 s later on a view already turned: the player moves
  backwards. Press them for the view the hands will show when they land (`view_when_keys_land`,
  a copy of the hands stepped the keys' delay ahead); a longer horizon skips the strafe key.
- A landing that leads to no item may be the step a later round bridges on foot (dm2's z 472
  ledge to the GL ledge): rank landings (leading to an item, then on target), never refuse them.

`se-reach` prints each map's rocket links and how many are flips; dm2's GL ledge is reached by
standing flips (no room to run there), and every map's `se-team` digest is unchanged.
