---
title: Ask the nav "can a bot get to every item and back" per map before tuning how bots fight; water, pockets and rocket jumps are links, not special cases
date: 2026-11-07
scope: plugins/shooter/nav.cpp, plugins/shooter/swim.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/map_nav.cpp, games/shooters_edge/bots/map_plan.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [se-reach, reachability, swim-node, add-swim, bridge-pockets, rocket-jump, trap, pocket, map-plan, lane, room]
---

`se-reach` lists, for every item of e1m2 and dm1 to dm6, whether a route leads to it from the
spawns and one back. It found 26 of 249 items out of reach, every one a nav gap: water closed to
routes (handled before by a dive per item and a swim field out), stairs or ledges no single
proof climbs, and a ledge only a rocket jump or a train reaches. Making the water swim nodes in
the graph, bridging each pocket with wider proofs (up to 384 across, a jump that lands anywhere
in the pocket kept) and rocket jumps as the last way took all seven maps to every item but dm2's
train ledge, the build still about 1.5 s a map. What broke on the way, each found in one trace:
a swim link down to a pool's floor read as a climb out (the bot bobbed at the surface), arrival
in water needing the ground, swim links priced faster than bots swim (routes through water
everywhere), and the map plan counting water as ways in and regions (its base moved off the
yellow armour). Price a swim at the speed bots really swim (128 units/s), judge the plan on dry
floor, mark nodes no way on foot leaves (`MapNav::trap`) and keep rocket jumps to pockets with an
item and a way back. A tool that reads a file mid-write crashes: never edit while precheck lints.
