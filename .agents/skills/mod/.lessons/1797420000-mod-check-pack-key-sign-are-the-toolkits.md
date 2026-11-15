---
title: mod check, pack, key and sign are the toolkit's; ge adds new, build and --run
date: 2026-11-15
scope: engine/toolkit/mod.cpp, engine/cli/mod.cpp, games/shooters_edge/cli/spec.cpp
concepts: [mod, toolkit, secli, gamecli, kmodgames, offers, needs]
---

The bodies of `mod check`, `mod pack`, `mod key` and `mod sign` and the table of games that offer
mods (`kModGames`) live in `engine/toolkit/mod.cpp`, so `secli mod check` and `ge mod check` are one
body; a change to the loader's findings or to a manifest merge is made there. `engine/cli/mod.cpp`
keeps what needs a game repo (`mod new`, `mod build`) and `--run`, which hands the package to the
game's CLI after the shared check. A game's CLI lists the mod rows only when its Spec offers
"mod" (`Spec::offers`); a new game that loads mods adds that word and its row in `kModGames`.
