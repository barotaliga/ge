---
title: A server browser lists only what the server info says: put the map and rules in it
date: 2026-11-07
scope: games/shooters_edge/net/classic_server.cpp, games/shooters_edge/net/classic_browse.cpp, engine/net/server_list.cpp
concepts: [server-browser, status-query, serverinfo, map, deathmatch, teamplay, needpass, golden]
---

The first `se-browser` run listed every server as `single` with no map: our server's info carried
the host name and the slots but not `map`, `deathmatch` or `teamplay`, which every other server of
the protocol sends and every browser reads. They are set where the info is built
(`classic_start`), with `needpass` when a password is set and `skill` only where monsters play it
(a match's `skill` is the mod's leftover default and lists as noise). The info's bytes go to every
client in the signon, so the classic session's and the team game's goldens moved
(`GE_GOLDEN_WRITE=1`) and the `se-*` labs' digests with them. A status reply has no nonce: match
it by the address it came from, and take a reply from an unknown address only inside the LAN
broadcast's window.
