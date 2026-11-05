---
title: In the sequel's move a player at rest stands an eighth above the floor
date: 2026-11-05
scope: plugins/shooter/sequel_move.cpp, plugins/brush/trace.cpp, tests/shooter_sequel_move_test.cpp
concepts: [sequel-move, snap, jitter, position-test, start-solid, eighths, landing]
---

The brush trace counts a box touching a plane as inside the brush. So an origin of exactly 24 over a floor at 0 is all solid. The slide stops 1/32 above the plane (24.03125). Packing truncates that to 192 eighths (24.0), which is solid, so the jitter nudges it toward its sign, to 193 (24.125). A sequel test that expects a resting player at z 24 is wrong. Start a scenario at 24.125, and expect 42.125 on an 18-unit step.

The landing pause (`kSqTimeLand`) is rarer than its rule reads. A command whose slide reaches the floor clips the fall's velocity away, to about +1% of it after the 1.01 overbounce, before the closing categorise. The pause starts only when a command ends within 0.25 above the floor while still falling. Test it from such a state. Do not assume it follows every high fall until the parity vectors say so.
