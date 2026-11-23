---
title: A Quake mod's default.cfg in its pack runs as what it adds; its quake.rc gives only the title's demos
date: 2026-11-23
scope: games/shooters_edge/client/install_configs.cpp, games/shooters_edge/client/main.cpp
concepts: [mission-pack, default-cfg, quake-rc, startdemos, menu-attract, install-configs]
---

Quake's start does not run the `default.cfg` inside the base game's packs (the window's own layout
stands), so a mission pack's copy, which repeats the base's file with a few lines added (keys 9
and 0 for its new guns, aliases), was never read either. `run_mod_default_additions` runs only
the mod's lines that the base's copy lacks, compared word by word in lower case, so the pack's
`unbindall` and its arrow layout never land. The window never runs `quake.rc`. A pack's
`startdemos` line is read by `mod_startdemos` and used where `menu_attract` is `auto`. Its other
lines (`exec`, `stuffcmds`, a stray `teamplay 0?Hipnotic`) are the start the window already runs.
To see a pack's title, run `se-attract --basedir=<install> --game=<folder>` (`--demo=title`, the default).
