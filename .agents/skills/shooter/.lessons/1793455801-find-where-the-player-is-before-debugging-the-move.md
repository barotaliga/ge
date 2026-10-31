---
title: Before debugging the move, find where the player really is against the brushes
date: 2026-10-28
scope: tests/golden/shooter_recording.txt, games/shooters_edge/sim/arena.cpp, games/shooters_edge/server/host.cpp, tests/shooters_edge_listen_test.cpp
concepts: [recording, scene, scenario, arena, roof, rocket, knockback, trails, rocket-jump, fixture, lintel, scripted-run]
---

Three "movement bugs" were the player being somewhere else:

- A recorded scenario failing from one line on while its twins pass may sit under the wrong scene:
  `ramp_down_slidefix` followed the 10 degree ramp's scene but its landing height (147.80 + 24 +
  1/32) named the 30 degree one. Compare the first contact's height with each scene's floors.
- Eight bots firing rockets threw players over the open test arena's walls, and the tables read 90 %
  of frames airborne. The server builds the arena ROOFED (`build_arena(a, true)`, the roof the last
  brush so the match's golden is unchanged). Look at the trails picture before the tables.
- A scripted rocket jump "rose 72 units" because the run drifted on after its keys let go and a
  doorway's lintel stopped it. Print every tenth frame's position; vertical speed going from +600
  to −90 in a tenth of a second is a ceiling, not gravity.
