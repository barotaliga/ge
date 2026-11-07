---
title: The game's own wire messages ride an info key, never the challenge, and leave recordings
date: 2026-11-07
scope: games/shooters_edge/net/classic_ext.hpp, games/shooters_edge/net/classic_frame.cpp, games/shooters_edge/net/classic_record.cpp, games/shooters_edge/client/demo_client.cpp
concepts: [ge-ext, extension-family, named-effects, svc-id, interop, recording-strip, info-key]
---

To add a message only our own client reads, negotiate it like the z set: `*ge_ext` in the
client's user info and the server's server info (a real server forwards an unknown star key from
the connect harmlessly; keep it short), so the challenge, the connect lines and serverdata stay
byte-for-byte classic. Pick an id far from every family's (90 and 92 are taken; 120 and 121 are
the game's), send it only where `c.ext` agreed, accept it in the demo reader only on the live
stream (`Demo::named_effects`), and strip it in `record_packet` unless the recorder is the
remote view's own, so a saved demo stays readable by classic players. Adding `*ge_ext` moves
the classic session's golden (`tests/golden/shooters_edge_classic_server.txt`) and the labs that
digest a session's bytes: rewrite them in the same PR.
