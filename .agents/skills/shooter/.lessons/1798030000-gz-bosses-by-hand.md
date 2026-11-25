---
title: A boss whose moves branch frame by frame is played by hand from a gz hook, not squeezed into roster attacks
date: 2026-11-23
scope: games/shooters_edge/server
concepts: [boss, carrier, widow, roster, attack, pose, shown, spare, brood, untouchable, tracker]
---

- **Roster attacks are a wind-up, a loop and a recovery.** A boss that picks its next frame by
  where its enemy stands (the widow's turret blaster), holds a frame until it faces a bearing (the
  carrier's launch) or re-chooses on a loop's last frame does not fit them. Give its row no
  attacks, play its moves in its own file (`q2_gz_bosses.cpp`: a move table of first frame and
  count, one event switch a frame, `m.pose.shown` for the frame) and return false between moves so
  the base run carries it. The base `choose` then does nothing for it.
- **A brood rides the commander's spares.** `free_spare` and `become` (q2_gz_monsters.cpp) take any
  spawner: spares made with the map under `block = -2 - owner`, re-kinded at the grow, counted in
  the owner's `called` and given back at their death by the same hook. A boss's spares are made
  by `gz_boss_spares` from `gz_spares`.
- **A tracker's prey is a player's slot or `-2 - entity`.** `m.prey < kMaxClients` is true for a
  monster too: test `prey >= 0` before indexing the players (UBSan caught it when a widow's tracker
  hit a stalker; a player's disruptor on a monster did the same).
- **A lab hall needs walls.** A tongue that pulls and a crunch that kicks threw the lab's player off
  the floor's edge: a 823-point step in the damage chart was the fall out of the world, not a hit.
