---
type: Added
title: Players turn to dust when they teleport away and are blown back in when they arrive
---

In Shooter's Edge a player who steps into a teleporter now crumbles where he stood, from his back to his face, into ash the wind scatters, each flake on its own way; one who spawns, respawns or comes out of a teleporter is blown in as ash that lands on his skin as it forms, face and head first. Your own arrival shows the ash gathering ahead of your eyes and the weapon assembling in your hands. It is a classic-enhance switch, `ce_warps` (TELEPORT DUST under WATER, LAVA AND SKY), on in the modern look; off, or in the classic look, a teleport and a spawn show the original fog with the player there or gone at once. The dissolve is the engine's: `MorphWarp` on a morph draw and a held weapon's draw (`engine/render/morph.hpp`, `engine/render/viewmodel.hpp`) crumbles any vertex-animated model to ash by one share, with knobs for a glowing edge, a pinch into a column and a shimmer.
