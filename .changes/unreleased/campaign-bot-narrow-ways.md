---
type: Fixed
title: The campaign bot finds ways narrower than its map's grid
---

The bot's map of a campaign level now has places in corridors too narrow for its 32-unit grid,
where the room a player has falls between two rows of the grid. Before, a room whose only ways out
were that narrow was sealed on the bot's map. Scourge of Armagon's hip2m3 left him in the start
arena for the whole run. Now he leaves it and goes from 5 to between 12 and 21 of 100 kills.
Dissolution of Eternity's r1m5 goes from 5 to 20 of 84. Such a place is added only where the map's
grid stands in rock, never at a chasm's edge, and in the middle of a way that narrow, so a door's
lip that stays in an opened doorway leaves him room to pass.
