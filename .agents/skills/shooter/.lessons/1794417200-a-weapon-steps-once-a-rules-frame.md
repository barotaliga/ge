---
title: The sequel's weapon steps once a rules frame, by the first command that presses attack
date: 2026-11-11
scope: games/shooters_edge/server/sequel_server.cpp, games/shooters_edge/server/sequel_fight.cpp, games/shooters_edge/server/sequel_combat.cpp
concepts: [sequel, weapon-frames, hand-step, stepped, period, armour-save, float-rounding]
---

The hand's step (`sq_hand_step`) runs once per 100 ms rules frame: a command that presses attack
steps it at once (so a tap between two frames still fires) and sets `SequelPlayer::stepped`; the
rules frame steps every hand no command stepped and clears the flag. Stepping in both places
without the flag fires twice a frame and halves every period. Keep the frame logic in
`sequel_combat.cpp`, free of the map, so a period is tested by counting steps
(`tests/shooters_edge_sequel_combat_test.cpp`): a period is (last fire frame − last activate
frame + 1) steps because a firing weapon turns ready one step after its last fire frame.

Armour's save is the share times the damage in floats, rounded up: 0.3 × 100 is 30.000002 in
floats and saves 31, not 30. Tests that read health after a blow through armour say so.
