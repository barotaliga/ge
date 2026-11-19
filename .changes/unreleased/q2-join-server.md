---
type: Added
title: Shooter's Edge's window joins a server of Quake II as a player
---

With Quake II loaded, `connect <host>[:port]` (port 27910 when none is given) joins a server of Quake II's protocol, a stock one running its own game or a mod, or ours (`sesv -game 2`), and plays on it: walking, turning, the weapon keys (sent as the game's own `use`), chat, votes, the game's own console words (`inven`, `drop`), `netstatus` and `disconnect`. The other players and things are drawn between the server's frames, so nothing steps at 10 a second. The map, models, sounds and player figures the window lacks are downloaded as it joins, into the archive they came from.
