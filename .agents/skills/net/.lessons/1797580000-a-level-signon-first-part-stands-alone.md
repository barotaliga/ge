---
title: A level's first signon part stands alone, and a recording may mark only one stage
date: 2026-11-14
scope: games/shooters_edge/net/q1_tv_netgame.cpp
concepts: [netgame, single-player, signon, reliable, recording, tv]
---

The single-player client loads the level inside the server info's parse and drops the
connection if any other reliable message arrives while it loads, so the face sends everything up
to the first stage as ONE reliable message (sealed: nothing joins it). Baselines may ride in that
same message: the client parses the rest of it after loading. A recording does not always mark
each stage: the demo writer's fixtures carry one `signonnum`, so the face cuts at the stages it
finds, gives a missing stage the next marked boundary (or the last), drops the recording's own
`signonnum` messages and appends its own 1, 2 and 3, one per part. Test with a scripted client
that resends its own reliable commands after a second, or a lossy link stalls the test client,
not the face.
