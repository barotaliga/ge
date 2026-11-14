---
title: A classic viewer at the TV needs its downloads refused, and a recording's silences cut
date: 2026-11-13
scope: games/shooters_edge/net/classic_tv_udp.cpp
concepts: [classic-tv, old-clients, signon, download, client-recording, black-box]
---

An existing classic client that lacks a player's skin asks for it (`download skins/base.pcx`)
during the signon and waits for the answer, so a face that ignores the command leaves the viewer
on the loading screen. Answer with the refusal (svc 41: size -1, percent 0) and the client goes on.
A bot's client recording may hold a long silence between its signon and its first frame (24 s
while the bot loaded its routes), so a recording played at its own pace looks hung: cut a
silence longer than a second. To make a long recording for a black-box run, use
`shooters_edge_bots --out=<folder>`: each bot's client recording lands there. With several bots
from one host, only one got in ("Bad challenge"), because our server keeps one challenge per host.
