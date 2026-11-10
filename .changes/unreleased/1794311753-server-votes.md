---
type: Added
title: Players vote on a server's settings for one map
---

Every game's server now takes players' votes on its settings (`engine/server/vote.hpp`): `vote fraglimit 30` asks everyone, `vote yes` and `vote no` answer, and the change is made the moment more than half of the people on the server say yes. It lasts until the map changes, then goes back. `sv_vote_allow` and `sv_vote_deny` choose which settings may be voted on (an empty allow list allows all but the deny list, whose default keeps passwords, rcon, the server's name, slots and address and the vote's own settings out); `sv_vote`, `sv_vote_time` and `sv_vote_wait` turn it off, time a vote and slow a caller whose vote failed. Shooter's Edge's servers, dedicated and the window's own, take votes on the mod's rules at once and on the move's settings with a map restart; the window sends `vote`, `yes` and `no` to the server it is on. The `server-vote` lab charts a session of votes.
