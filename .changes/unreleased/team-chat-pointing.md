---
type: Added
title: Team chat says what you point at
---

In Shooter's Edge, `%t`, `%x`, `%y` and `%j` in a team message now name what the player points at (an item, an enemy, a teammate, a glowing enemy) and where it is, and `tp_msgpoint` sends it to the team. `tp_msgenemypwr`, `tp_msgsafe`, `tp_msgkillme`, `tp_msgutake` and the quad and pent messages read it too. It works in the window's own game, on a joined server and in a demo. `tp_pointtimeout` sets how long `%j` remembers it.
