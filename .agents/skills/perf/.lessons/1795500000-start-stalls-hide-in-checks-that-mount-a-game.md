---
title: A start-up stall can be a yes-or-no check that mounts a whole game; ask the archive's directory
date: 2026-11-25
scope: engine/prof/frames.cpp, engine/vfs/search.cpp, games/shooters_edge/client/installs.cpp
concepts: [profile, frame-profile, start-stall, hitch, vfs, game_has_file, read_from_pack, perf-timeline]
---

- `profile N` (the console) names a slow frame's dearest zones, but a stall outside every zone
  reads only as `(no zone)`. Find it with a main-thread perf timeline: `perf record -e cpu-clock
  -g`, then `perf script --tid <main> -F tid,time,ip,sym` bucketed by time; never uprobe malloc
  with dwarf stacks (the files ran to 50 GB).
- The game list's edition check (`edition_of`) mounted each known install's base folder, reading
  every pack whole (251 MB for Quake and Quake II) on the main thread at start and again when
  the list refreshed, to learn whether ONE file exists. `vfs::game_has_file` reads only the
  packs' directories. Any "does it hold X" question goes through it, never `add_game`.
- In the container steady play is the software GPU's (`frame.present`, `world.submit`); the
  game's own CPU work is about 4 ms a frame. Judge the CPU side by zones, the GPU by the Mac's
  `gpustats`.
