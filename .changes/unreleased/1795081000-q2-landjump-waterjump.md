---
type: Added
title: Quake II's server lifts the landing pause and the surfacing jump, and limits slots per address
---

`sv_landjump` lets a player jump again at once after a fast landing, and `sv_waterjump` leaves jump held under water to the swim's own up move instead of a fixed lift. Both are on by default for players whose game is told the server's move and so predicts it; 2 turns them on for everyone, 0 keeps the stock move. Quake II's server also takes `sv_iplimit`, 3 by default, so a fourth connection from one address is refused.
