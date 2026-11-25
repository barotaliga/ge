---
type: Changed
title: Coop weapons stay and plats wait in both classic games
---

`coop_pickup_weapons` and `coop_elevator_delay` are now settings both classic games share, with the same defaults. In Quake's coop a weapon stays on the map for the other players: each player takes it once, and with `coop_pickup_weapons 1` a player who already holds it may take a copy nobody has taken yet, for its ammunition. A plat stepped on in coop waits `coop_elevator_delay` seconds (1 by default) before it rises, so the others can board.
