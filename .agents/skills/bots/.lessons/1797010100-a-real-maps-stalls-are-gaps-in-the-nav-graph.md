---
title: Every place a bot stalls on a real map is a gap in the nav graph; state what a link is, ask se-reach for every item, script a key place
date: 2026-10-29
scope: plugins/shooterbots/nav.cpp, plugins/shooterbots/nav_script.cpp, plugins/shooterbots/pilot.cpp, games/shooters_edge/bots/map_nav.cpp, games/shooters_edge/bots/item_routes.cpp, games/shooters_edge/labs/se_reach_lab.cpp, games/shooters_edge/labs/se_spot_lab.cpp
concepts: [nav, proof, prove-link, se-reach, reachability, pocket, teleporter, button, start-open-door, scripted-route, se-spot, treads, stairs, slopes, swim-node]
---

- A proof that asks only "did the move get there" let a jump land, walk up the stairs and count. Say
  what each kind IS: a jump is one flight; the "no nearer" stall counts only on the ground (a walk
  off a ledge overshoots in the air). Try a jump only where walks take over 1.5 times a straight run
  (proofs 36 082 -> 5 232).
- The bots that passed the fixtures stood idle half a real map's match. Each stall was a place the
  graph does not describe, found by the lab's last-state notes: a door a button opens (`"health"`
  nonzero is shot, zero touched); a goal outside the bot's own reach (filter goals by it);
  teleporters (`nav_add_links`); water (swim nodes priced at the 128 u/s bots swim); a door that
  starts open (a solid floor, never a mover).
- `se-reach` asks each item "a route there and back from the spawns": 26 of 249 on seven maps were
  out, each a nav gap.
- The 32-unit grid steps over a stair tread narrower than itself; `se-spot --at=` draws the place
  and shows it in one run. A denser graph cost frags elsewhere: script the key place (`nav_script`)
  and enter it as its proof starts (lined up, under 330 u/s, within 30 degrees), or bots arrive at
  hop speed and orbit the entry.
- `level()` with a step's 18 units read e1m2's stairs as climbs and bots circled each stair node
  (`slopes`). When a bot circles a node, read the pilot's arrival rule before its steering.
