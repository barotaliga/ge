---
title: A Quake II level change carries the new count in the server data, not only the prompts
date: 2026-11-13
scope: games/shooters_edge/net/q2_net_server.cpp, games/shooters_edge/net/q2_game.cpp
concepts: [level-change, server-count, loading-picture, map-list, cvar-kind]
---

After `changing` and `reconnect`, an unmodified client signs on again and takes frames, but it
keeps its loading picture up until the server data names a count other than the one it had: the
signon prompts' count alone is not enough (`q2_net_level` raises `cfg.wire.server_count`,
which `q2_signon_parts` writes). Found by recording a stock server's level change through a
logging UDP relay and diffing it with ours. Also: a `CvarSpec` without `.kind` is a number, so a
text setting such as `sv_maplist` read as `0` until it was given `CvarKind::text`.
