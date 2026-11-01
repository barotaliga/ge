---
title: A mode's program is found by the game profile's file names, form checked, never by a fixed name
date: 2026-11-01
scope: games/shooters_edge/profiles/game_profile.hpp, games/shooters_edge/client/listen.cpp, games/shooters_edge/client/main.cpp
concepts: [game-profile, program, qwprogs, progs, multiplayer, single-player, find-program, mod-folder]
---

Classic multiplayer mods ship `qwprogs.dat`, and some ship `progs.dat`. The base game's `progs.dat` is the single-player campaign. So the file name alone does not say which mode a program is for. `find_program` (client/listen.hpp) takes the game profile's list for the mode, looks each name up through the search path (a mod's folder over the base), and keeps the first whose header form (the layout CRC) is the mode's. It names every file it passed over in `why`.

Never write `"progs.dat"`, `"maps/start.bsp"` or `"gfx/pop.lmp"` in the window again. They are `GameProfile::*_programs`, the content row's `start_map` (with `asset_path`) and `full_game`. The dedicated server's `base_dir` is still a literal, and `tests/shooters_edge_game_profile_test.cpp` holds it equal to the content row's.
