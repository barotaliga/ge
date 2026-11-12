---
type: Added
title: The sequel's items stand on its maps and are picked up
---

The sequel's rules spawn every item on its maps by class name (armour, power armour, weapons, ammo, health, powerups, packs and keys, from one table in `server/sequel_items.hpp`), leave out those its spawn flags exclude in the mode, drop them to the floor, and give them to a player who touches them by each kind's rule: carry limits, armour converted by its share, the mega health counting down, and powerups held to the skill. In deathmatch a taken item shows again after its respawn time. Lab `se-sequel` draws each map's items from above.
