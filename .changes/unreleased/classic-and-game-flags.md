---
type: Changed
title: -classic picks the game, -game a mod's folder
---

`se`, `sesv` and `setv` pick the game by its id: `-classic q1` (Quake, the default) or `-classic q2` (Quake II), so a game added later is one more id. A mod's folder is `-game <folder>`, as each classic game names one; it replaces the window's `-mod`, and on `sesv` it is the same as `--mod=<folder>`. `secli` takes `--classic=<id>` and `--game=<folder>`. The numbers `-game 1` and `-game 2` are gone.
