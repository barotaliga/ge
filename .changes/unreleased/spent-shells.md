---
type: Added
title: Spent shells under classic enhance (ce_shells)
---

With classic enhance on, the guns that fire cartridges throw out their spent cases: Quake's shotgun a red hull as the slide comes back and the double barrel two as it breaks open; Quake II's shotguns their hulls and its machine gun, chain gun and the monsters' guns their brass (the tanks' and bosses' big guns heavier brass). The cases bounce off the map, lie down and roll, clatter by what they land on, lie a while and fade. Another player's and a monster's guns throw theirs too. `ce_shells 0` (also on the menu's weapon page) throws none. The engine gains a thrown-debris pool (`engine/detail/debris.hpp`) any game can use. Quake II's held shotguns now show their muzzle fire and kick on every shot (the hand missed shots fired on a weapon's first frame).
