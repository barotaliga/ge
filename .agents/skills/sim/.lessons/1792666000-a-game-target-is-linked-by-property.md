---
title: A game's simulation is one static library the labs and the tests link through GE_GAME_TARGETS
date: 2026-10-21
scope: games/fall_line/CMakeLists.txt, CMakeLists.txt, tests/CMakeLists.txt
concepts: [cmake, game-target, game-library, labs, tests, link]
---

`games/fall_line/CMakeLists.txt` builds `games/fall_line/sim/*.cpp` into `fall_line_sim` and
appends it to the global `GE_GAME_TARGETS`, which the root build reads when it links the `labs`
executable and `tests/CMakeLists.txt` reads for every test. The games are added before the labs
and the tests, so the property is filled by then. A new game does the same; nothing is listed by
hand. A game's labs (`games/<game>/labs/`) are still compiled into `labs` itself, never into the
game's library, because their registration is a static initializer.
