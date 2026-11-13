---
title: The sequel's own demos are an older protocol; record one to test the TV
date: 2026-11-13
scope: games/shooters_edge/net/sequel_tv_demo.cpp
concepts: [sequel-tv, demo, dm2, protocol-34, black-box]
---

The full game's `demos/demo1.dm2` and `demo2.dm2` carry protocol 26 in their server data, and an
unmodified current client sends `disconnect` as soon as it reads it. To try a recording through the
sequel TV with a stock client, record one first: `labs se-sequel-net --server=<a sequel server>
--seconds=30 --record=<file.dm2>`. Also: a face that clears a viewer's queue for a new level must
let that viewer be sent the next message (`go`), or the change notice sits unsent.
