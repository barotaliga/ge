---
title: An elevator moves only for its rider, so its shaft and its boarding depend on where it stands now
date: 2026-11-28
scope: plugins/shooterbots/nav_elevator.cpp, plugins/shooterbots/pilot_plan.cpp, plugins/shooterbots/pilot.cpp
concepts: [elevator, moving-platform, shaft, car, button, new-plat, dissolution]
---

- A car sent a floor at a time by buttons on its top is a moving platform with a stop per floor
  that never moves by itself. Ride it by walking into the button beside its top while aboard
  (pilot.cpp's ride branch, `press`); the ride's cost is the climb plus a press per floor, not the
  round trip a train's track gives.
- The nav proves links without movers, so any link across the shaft is proven with the shaft
  empty. With the car up, a drop from the top landing down the shaft lands on the car: re-prove
  such links with the car at each floor (`nav_cut_shafts`). Skip a floor that puts the car where
  the link starts, or the pit under the car's bottom floor loses its way out (the bot waited there
  130 s).
- The car stays where its rider left it, so a plan that boards it at another floor waits for good.
  Cut those boarding links from each plan, by where the car stands now (`cars_away`).
