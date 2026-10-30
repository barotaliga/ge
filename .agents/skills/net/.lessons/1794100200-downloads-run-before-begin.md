---
title: A classic client's downloads come before begin, so the frame must carry them while connected
date: 2026-10-30
scope: games/shooters_edge/net/classic_frame.cpp, games/shooters_edge/net/classic_signon.cpp
concepts: [classic-protocol, downloads, chunked, signon, skins, send-frames]
---

The client that asked for its extensions through `pext` stalled in the signon. It asked for every
chunk of a skin over and over, and no chunk ever came. The skins step comes before `begin`, so a
downloading client is still only connected, and the frame writer had put the chunks inside the
spawned-only block (beside the player records and entities). `send_download_chunks` now runs for
every client the server replies to. The clients that asked through the connect line passed only
because no one else wore a skin they lacked when they joined.

A trial with one client hides this kind of bug. The server test and `se-net` run three clients,
one for each way of asking for extensions, each wearing a different skin.
