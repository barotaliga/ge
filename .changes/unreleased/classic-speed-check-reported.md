---
type: Added
title: A speed hacker on a classic server is flagged in the match report
---

Shooter's Edge's classic server counts what its speed check cuts off each player's commands (`ClientSlot::speed_cut_ms`) and scores it once a second on the engine's guard (`guard_flag`, a game's own check). A player cut by more than 50 ms in each of five seconds is out, and the match report names them (`flagged`), so a master that trusts the server bans the account. The `se-cheats` lab runs fast clocks, an honest player, a stall's burst and a bot.
