---
type: Fixed
title: In Quake II a player caught a fraction inside a monster walks out of it
---
A player's move snaps the player to an eighth of a unit, and the snap could leave the player a fraction inside a monster's box. A box that started inside another was met at once whichever way it moved, so the player and the monster stood locked together for good. Now a move that leaves the box is free, and only a move that ends inside it is held.
