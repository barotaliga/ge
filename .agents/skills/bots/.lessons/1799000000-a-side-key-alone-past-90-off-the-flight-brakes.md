---
title: A side key alone with the view led into the turn brakes the flight; keep its wish within 90 of the velocity
date: 2026-11-26
scope: plugins/shooterbots/pilot.cpp, plugins/shooterbots/pilot.hpp, tests/shooterbots_pilot_air_test.cpp
concepts: [air, air-strafe, side-key, wish, brake, jump, flight, late-hands, lead]
---

- The side key's wish lies 90 degrees off the view, so leading the view INTO the turn puts the
  wish past 90 off the velocity: Quake's air then accelerates against the flight with the whole
  wish speed (44 units/s a command), and a 302 units/s jump stopped in a tenth of a second
  (e1m4's slime pit). Leading into the turn is right only with forward held too (a 45 degree
  wish); with the side key alone the view sits a few degrees BEHIND the turn (`air_strafe`).
- A late hand does not excuse a lead: the bot turns the pilot's keys under the hand's view
  (`turn_axes`), so the wish's world direction is the pilot's whatever the hand lags.
- Read a flight's keys in the campaign lab's frames (`--frames`): `cmd` forward -400 in the air
  on a jump is a brake. A seed's run diverges within a minute, so judge a bot change on seeds 1
  to 3 (`--seed`), not one run.
