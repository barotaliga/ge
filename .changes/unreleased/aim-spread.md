---
type: Added
title: The aim spread, a bot-likeness number in a shooter's match report
---

`plugins/shooter/aim_spread.hpp` measures where a firing player's line crosses the players it is on and how tightly those points sit: an aim bot held on one point reads near 0, a hand about 0.6 to 0.8. Shooter's Edge's classic server keeps it per player, and the match report carries it as the `aim_spread_min` column, a number to read beside the player rather than a ban. The `se-aim-spread` lab pictures aim bots against hands and reads the server's own bots in a match.
