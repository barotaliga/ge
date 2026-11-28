---
title: A jump braked to a stop in mid-air with the pilot strafing is the keyboard's brink guard
date: 2026-11-28
scope: plugins/shooterbots/keyboard.cpp, plugins/shooterbots/pilot.cpp, plugins/shooterbots/nav_harm.cpp
concepts: [brink, brinkkeys, air-brake, capped-air, jump, lava, slime, keys, frames]
---

- The campaign's `--frames=<file>` line gives the command's keys after every stage. When the keys
  disagree with the pilot's `because` (strafing, yet forward -400), add `keys_by` to the line: the
  keyboard's `brinkkeys` (keys_off_brink) runs after the pilot and is marked "bot".
- Quake's capped air caps only the speed a key adds along its wish. A key against the run takes 44
  units/s off each 14 ms command. No guard may press keys in a planned jump's flight in either air.
- A guard whose step (64 units) is longer than the distance to a planned jump's start at a pit's
  lip lets go of W short of it. Exempt the walk's last steps to that start, not only the start.
- One change can move a deterministic run onto a different path that surfaces a latent stuck
  (e1m4's gate). Rerun the base seeds after every pilot or keyboard change, and print the keys
  the guard dropped (in, out, the movers near) to see the cause.
