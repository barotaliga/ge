---
type: Added
title: Shooter's Edge fighters play the baseline mod's deathmatch
---

`games/shooters_edge/bots/fighter.hpp` is a bot that fights on the baseline QC mod from what its client has (its own stats, the map's items timed off what is sent, its senses), choosing its mode, item goal and weapon on `ai/choose` and when to fire; `run_fight` plays four of them on the arena, and the `se-bots` lab pictures the match. The bot's senses now drop a dead player, and its aim leads a projectile by its flight time.
