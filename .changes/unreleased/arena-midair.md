---
type: Added
title: Midair plays on the first game's arena, and map-intent plays midair matches
---

`k_midair 1` in deathmatch now plays midair on the first game's arena as it does on Quake II's rules: each spawn gets 250 health, 200 red armour, the blade and the rocket launcher, and rockets are never used up. Nothing lies on the map and nobody drops a pack. Another player's rocket on a target at the least height or more (`k_midair_minheight`) kills through any armour and scores its band's frags, told to all ("Silver midair by bot1 on bot2 (264 up)"). Any other blow wears the armour only, and the dead come back after 2 s. The server no longer refuses the vote on the first game's maps, and `k_midair` and `k_midair_minheight` are server settings there. The `map-intent` lab plays the `midair` mode with four bots. The number rules of `plugins/shooter/midair.hpp` are now header-only.
