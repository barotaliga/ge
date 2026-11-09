---
title: The window's match is the baseline mod even on the classic packs; check its own QC before blaming the move
date: 2026-11-09
scope: games/shooters_edge/qc/common/player.qc, games/shooters_edge/client/game_folder.cpp, games/shooters_edge/labs/gpu_se_swim_lab.cpp
concepts: [baseline-mod, player-frame, body-cycle, swim, find-program, progs, listen-server]
---

A report of "players do X wrong in the classic game" is usually about the baseline mod. The base pack's `progs.dat` is a single-player program, so `find_program` passes it over for a match, and without a `qwprogs.dat` in the folder the window's deathmatch runs the baseline mod (`load_programs` says which in its status line). The pack's own program drives the body correctly in water (`se-swim --progs=pack`); the baseline mod set no body frame at all until `player_body`, so every player held frame 0.

Before changing the move or the client, run `se-swim` (or a probe like it) with `--progs=pack` and without it: the same scenario on both programs tells a mod's gap from an engine's. The player's frame is the program's alone; the client draws the byte it is sent, blended at 10 Hz.
