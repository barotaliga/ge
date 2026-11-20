---
type: Added
title: Classic remix hears matches with either game's sounds
---

Shooter's Edge's classic remix gains `cr_sounds` (empty the played game's, `q1` Quake's, `q2` Quake II's): every multiplayer sound (shots, blasts, pickups, jumps, pain, deaths) is heard as the chosen game's by a table of matching sounds. Swapping the items with `cr_items` no longer changes how they sound, so the other game's weapons fire with the played game's shots unless `cr_sounds` says otherwise. The `se-remix-sounds` lab shows every pair.
