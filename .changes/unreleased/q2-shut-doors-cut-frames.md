---
type: Changed
title: Quake II servers stop sending what lies behind a shut door
---

A Quake II server now leaves out the monsters, players and items behind a shut door, and their looping sounds with them, and the frame tells the client which rooms are open so it skips drawing the rest. Sounds and effects reach only the players who can hear them through open doors, a sound from a moving rocket or player follows it, and a long message such as a new scoreboard no longer costs a whole frame: the frame sends what fits.
