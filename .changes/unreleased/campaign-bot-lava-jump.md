---
type: Fixed
title: The bot's lava guard no longer brakes a jump it has planned over lava or slime
---

In Quake's air, the guard that keeps a bot's keys off a lava or slime pit stayed on during a jump
the bot had planned over the pit. A run carried over the pit then got the keys pressed against it.
Quake's air limits only the speed a key adds, not the speed it takes away, so this braked the
jump. On Scourge of Armagon's hip2m3 it stopped a jump over the lava cleft dead in mid-air, and the
bot fell in. The guard is now off for the flight of a planned jump, as it already was in Quake II.
It is also off for the last steps of the walk to a planned jump's start at the pit's edge. On e1m4
it had let go of the walk short of the start, beside the slime. A door sunk into its slot now
counts as floor for the guard. hip2m3 goes from 12 to 21 of 100 kills, and r1m5 from 20 to 23 of
84. e1m4 finishes with every monster and every secret at 620 s (it finished at 832 s with 37 of 42
kills before).
