---
title: In the sequel a released attack lands a rules frame after the last held one
date: 2026-11-11
scope: games/shooters_edge/server/sequel_server.cpp, games/shooters_edge/server/sequel_combat.cpp, tests/shooters_edge_sequel_fight_test.cpp
concepts: [sequel, hand-grenade, hand-step, stepped, release, intermission, test-timing]
---

The hand is stepped by the first command of a rules frame that presses attack; when attack is
let go, the frame whose commands already stepped it (`stepped`) skips the frame-end step, so the
release is first seen by the NEXT rules frame. A test that holds attack, then runs one frame's
worth of host ticks (8 × 13 ms) and looks for the thrown grenade finds nothing: run two frames
(16 ticks). The same holds for anything read off a release (the chaingun's early stop).

Two more traps from the same row: holding attack after a death respawns the player a second
later (attack is the respawn button), so a test that cooks a grenade to death checks the
obituary and frags, not `alive`, or stops pressing first; and a frag or time limit now starts
the intermission in the frame it is reached, which stands every dead player up at once.
