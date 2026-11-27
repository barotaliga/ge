---
title: Every guard that reads the static world is blind to a platform over lava; teach it the movers
date: 2026-11-27
scope: plugins/shooterbots/brink.cpp, plugins/shooterbots/keyboard.cpp, plugins/shooterbots/buttons.cpp, plugins/shooterbots/way_hands.cpp, plugins/shooterbots/pilot_jump.cpp
concepts: [brink, ferry, moving-platform, lava, poke, dodge, unwedge, floor-plate, button-pressed, pillar, ride]
---

- Quake II's movers are not solid in the bots' server mirror, so every check that traces it (the
  brink guard, the keys' brink, the dodge's and the unwedge's clear test) sees lava under a ferry.
  Exempting whole modes from the guards (on a mover by the route) let him run in beside the ferry
  instead; passing the movers as they stand (`PilotBot::now`) and counting a step onto a ridden
  top as floor (`nav_on_mover_top`) is the fix that holds both ways.
- After the pilot, several later stages can rewrite the keys: the button poke (walks at a button
  within 200 units, across a gap too), the dodge, and the unit run's unwedge (a side shove plus a
  jump after 1.5 s still). A jump that "fires from nowhere" with the pilot's `because` unchanged
  and `jump_pressed` false is one of them: print `keys_by` per frame. Waiting for a mover is
  stillness on purpose: `way_held` resets the unwedge's clock.
- A button whose travel is all lip never moves when pressed; the plan's "moved from rest" test
  never saw it. Its world row's frame or effects change instead (`button_looks_pressed`).
- A rising pillar ridden near its edge jams 40 units up under the shaft's rim with no message:
  ride a rising platform at its middle.
- The runs are long (10 min for 900 s); a per-frame print for a 1.5 s window (`t` from the
  rules) found each culprit in one rerun of ~2 min with `timeout`; strip it before the commit.
