---
title: The rules' world types live in plugins/shooter, the QC filler in the game
date: 2026-11-15
scope: plugins/shooter/rules_world.hpp, games/shooters_edge/server/rules_world.hpp
concepts: [rules-world, world-entity, seams, bots, shooter]
---

`RulesWorld` and its rows are `ge::shooter` types in `plugins/shooter/rules_world.hpp`; a field
any game's rules show goes there, and every runner fills it (the QC server in
`games/shooters_edge/server/rules_world.cpp`, the sequel in its own `fill_rows`/`fight_rows`). The
game's header only names them by using-declarations, so `server::WorldEntity` still compiles;
never forward-declare them in the game's namespace (the using-declaration would clash).
