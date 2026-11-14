---
title: Another game's figure wears its own palette, and the warp take pictures a figure fast
date: 2026-11-14
scope: games/shooters_edge/client/world_models.cpp, games/shooters_edge/client/remix.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [classic-remix, cr-models, player-figures, figure-frames, palette, pcx, se-client, warp]
---

A sequel figure's skin (`players/<folder>/*.pcx`) read in the classic window came out blue and
purple: `read_skin` coloured every PCX through the window's palette. A whole figure picture carries
its own 768-byte palette; use it (`IndexedImage::palette`) and the figure looks right in any game.
The figure's `weapon.md2` needs its adapter's stand-in pictures applied too (`own_pictures`). The
classic figure stands alone (`progs/player.mdl`, not under players/), so the sequel window reads it
as a made-up folder from `profiles::kRemixFigures`, its frames mapped from the first sequel figure's
by kind. `RemixFiles::wearing()` now also covers the figure alone, so anything that dresses the held
weapons must test `worn`/`looks` itself (a null `looks` crashed `load_worn_held`). Picturing another
player: a sequel coop companion on base1 builds its nav for ~25 minutes in the debug lab; `--warp=x,y,z,yaw`
with `--views` stands a figure where you want it on a deathmatch map in a minute or two.
