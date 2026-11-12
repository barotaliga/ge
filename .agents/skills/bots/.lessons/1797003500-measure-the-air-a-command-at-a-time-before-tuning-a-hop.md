---
title: Measure a hop's air a command at a time, by who turned the view, before tuning the hop
date: 2026-11-12
scope: plugins/shooter/bot.cpp, games/shooters_edge/bots/keyboard.cpp, games/shooters_edge/bots/buttons.cpp
concepts: [hop, air, airkeys, aimhop, airview, keys, mouse-hand, view-lag, double-lag, air-accelerate, speed-gain, se-team, trace, view_by]
---

- Every hop measure (hop share, takeoff speed, gain p50) said the bots' hop was weak; none said
  why. The horizontal speed's change a command in a hop's air, off an `se-team --trace
  --trace-every=1` grouped by the trace's `view` (who turned the view: aim, air, look, poke)
  and `key_set`, said it at once: the hop LOST 1.5 units/s every air command (people gain
  ~0.7). Use the horizontal speed (`vx`, `vy`): the `speed` column has the fall in it.
- The cause was a double lag: the pilot's own hands are a reaction late and lead for it, then
  the mouse hand follows that view with its strokes, so the view sat past the 7-degree window
  where a side key gains, and a side key turned under it brakes. Any view a bot did not press
  itself (the aim's, a watched spot's, a game's) leaves the pilot's wish braking in the air;
  choose the keys for the view actually pressed (`aim_air_set`), not the pilot's.
- Letting go of a braking key made the flight a plain arc that the enemy's aim reads exactly:
  faster, and -5.6 a match head to head. Valuing the curve the side key gives turned it round
  (+4.6). A speed fix can lose frags for a reason that is not speed; read the deaths per side.
- A game view writer that sets `cmd.angles` must turn the wish under it (`turn_axes`):
  `bot_hand_override` reads the wish as meant under the new view (the poke braked at -5.7).
- Keys land 1-3 commands after they are chosen: a set chosen for this command's view brakes
  when it lands, and even "no key" commands lose. Choose for the view when the key lands.
