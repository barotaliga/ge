---
title: A move that rounds the sent angles is not a forced view, and an aid needs the move's air law
date: 2026-11-12
scope: games/shooters_edge/client/listen.cpp, plugins/shooter/speedjump_assist.cpp, games/shooters_edge/server/sequel_server.cpp
concepts: [speedjump, easy-jump, sequel, forced-view, angles, air-accelerate, jump-up-move]
---

Carrying the easy jump to the sequel's move took three things the classic move had hidden. The
window compared the server's view with the sent angles exactly to spot a mod's forced view; the
sequel rounds every sent angle to 65536ths of a turn, so every frame looked forced and the window
took the sent (aided) yaw as the player's aim: the hops ran straight while the view turned. Compare
wrapped, with a tolerance above the rounding. The aid's wish angle came from the classic air law
(the 30 cap reached in one command); the sequel's default air adds the whole wish speed at an
acceleration of 1, so the best share along the way is `W - A`, not 0: pass the law in
(`SpeedJumpRules`) and derive the angle from it. And a slow air turns a flight less than the swing,
so the takeoff rule stuck the weave on one side and a far-off aim could not be reached in the air:
measure the flight's turn and steer on foot. The sequel reads its jump from the up move, so the
classic protocol's jump button must be mapped where the command crosses into its move
(`sequel_command`), once, for every caller.
