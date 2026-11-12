---
title: Where bots are wet and people dry in the same place, the nav lacks people's jump; read the map's gap tricks before pricing water
date: 2026-11-12
scope: games/shooters_edge/bots/gap_tricks.cpp, games/shooters_edge/labs/se_reach_lab.cpp, plugins/shooter/nav.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/fighter.cpp
concepts: [water, wet, swim, gap-tricks, gapbuild, se-reach, route-wet, se-rj, se-people, samples, circle-jump, run-up, runupkeys]
---

- e1m2's bots swam 27 % of their time, 72 % of it on routes planned through the water. Water was
  priced 2-4x before with no effect, and the reason only showed once both were compared by place
  (`se-people --samples` on people's recordings and on a bot match, 128-unit cells, the wet share
  and the height): in the cells where bots were wet, people were there too, but dry on the
  walkways 230 units above the flooded floor.
- `se-reach --route=... --route-wet=1000` prices every link into a wet node 1000 times: a route
  that still swims means the nav has NO dry way, and pricing water cannot help. Then ask people
  how they cross: `se-rj --dir=<demos>/<map>` writes `gap_tricks.tsv` (every jump people make over
  a gap, clustered, with the takeoff speed). e1m2 had 113 and the nav had none:
  `bots/gap_tricks.cpp` held only dm2's. Read every map's before tuning its water.
- A gap trick in the nav is not a gap jumped: a hopper plans one only at 380 u/s, which bots
  rarely reach, so `team.gap-jumps` stayed 0. Planning it from further out (`gapbuild`) took the
  routes dry (wet -2.7 points) but every one was refused at the lip at 230-330 u/s. People curve
  into these from ~150 units out at ~180 u/s and leave at ~490 (the samples, 0.1 s apart): a gap
  needs its own run-up, not a hop that happens to reach it. Count `gap-jumps` before judging.
- Keys worked out for the view WANTED are pressed against the view the hands still show: a
  stepped-back run-up turning round walked sideways off a walkway into the pool. Work keys out for
  the view when they land (`view_when_keys_land`). Fixed together with a replan on falling off
  (`runupkeys`), stuck rose 147 -> 198: split a change in two before calling it dropped.
