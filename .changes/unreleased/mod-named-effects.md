---
type: Added
title: Shooter's Edge draws a mod's own named particle effects
---

A mod may call `particleeffectnum`, `pointparticles` and `trailparticles` (the `DP_SV_POINTPARTICLES` extension). On the listen server the window draws an effect of a classic name as the classic effect and any other as the game's files' effect of that name (`particles/*.fx`); a classic client gets the classic temporary entity for a classic name and nothing for any other. The `se-weapon-fx` lab's `--mod-effects=1` pictures a fountain and a trail.
