---
type: Added
title: A game's player CLI checks, packs, keys and signs C++ mod packages
---

`secli mod check|pack|key|sign` work for a mod's author who has Shooter's Edge and not the engine, with the same bodies as `ge mod` (now in `engine/toolkit/mod.hpp`, with the table of games that offer mods); their answers name the program that ran them. `ge` keeps `mod new`, `mod build` and `mod check --run`. A toolkit row can need something of the game (`Row::needs`), and a game's CLI lists it only when its `Spec::offers` names it; `Spec::tools` is renamed `offers`.
