---
title: Ask a classic game's files through its adapter, never by its file names
date: 2026-11-01
scope: games/shooters_edge/client/game_adapter.hpp, games/shooters_edge/client/classic_adapter.cpp, games/shooters_edge/profiles/game_profile.hpp
concepts: [game-adapter, classic-game, mods, program-form, map-textures, demos, sequel]
---

Each classic game lays out its mods, programs, map textures and demos differently (one QC program
for the server; a native library; up to three bytecode programs, one a side), so the shell never
reads those files itself: it calls the game's adapter (`adapter_of(game)`), and the first game's
answers live in `classic_adapter.cpp` alone. Keep names (folders, file names, extensions) in the
profile rows and the logic in the adapter; a later game is a second adapter file, never a branch
inside the first's. A program file always says its form and side (`ProgramFile`); a form with no
runner is named in `why` and passed over (`runs_form`), never loaded as QC. To prove a call moved
without a change, check the scene's pictures are the same views `read_map_pictures` gives and that
the GPU labs' digests (`se-client`, `gpu-bsp`) hold. The adapter's calls take `self`, so a test can
copy the table, point `game` at a changed row and check a case no shipped row has yet.
