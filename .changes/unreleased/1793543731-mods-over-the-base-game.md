---
type: Fixed
title: a mod folder sits over the base game, and the full game finds its enhanced cache
---

Shooter's Edge's `-game <folder>` now mounts the mod's folder over the base game's instead of in its place, so a mod that ships only a few files plays with everything else from the base game, and its zips (`*.pk3`, `*.pk3dir`) are read too. The game's files go through the engine's search path. With the full game's later packs installed, the game now finds the textures, pictures and light classic-enhance made for them (it looked under the first pack's key alone).
