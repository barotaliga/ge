---
title: A game mode of our own is C++ every game's rules call, never QC
date: 2026-11-12
scope: plugins/shooter/midair.hpp, games/shooters_edge/server/sequel_fight.cpp, games/shooters_edge/net/classic_game.cpp
concepts: [game-mode, midair, no-qc, shared-rules, vote, classic-arena, sequel]
---

Our own game rules are written in C++ (Niclas, 2026-11-12): QC is only for running other people's
mods. A match mode (midair) is a pure rules file in `plugins/shooter` that each game's C++ rules
call at its own seams (a blow's damage and push, a kill's score, the spawn's kit, the items left
out). The classic arena is QC until its port to C++ lands, so a vote on a mode only its C++ rules
play is refused on the QC program (`classic_game.cpp` `vote_check`), while a loaded match mod
module keeps its own settings of the same name.
