---
type: Added
title: A QC compiler and Shooter's Edge's baseline deathmatch mod in QC
---

`ge qc build <progs.src>` compiles QC sources to a progs the VM runs, with every mistake named at its file and line (the compiler is `plugins/progs/qc_compile.hpp`). Shooter's Edge ships its game logic as a mod in source, `games/shooters_edge/qc/`: weapons, ammunition, armour, health, deaths, respawns, frags, the frag and time limits, doors, plats, buttons and triggers, for modders to copy and change; the `se-baseline` lab plays it against bots. The mod server now clears a forced view (`fixangle`) once it is sent, so a player can turn after spawning.
