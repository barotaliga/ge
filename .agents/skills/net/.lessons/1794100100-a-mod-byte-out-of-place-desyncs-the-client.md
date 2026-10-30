---
title: A mod's raw message bytes are checked before they reach a classic client
date: 2026-10-30
scope: games/shooters_edge/net/classic_frame.cpp, games/shooters_edge/server/test_mod.cpp
concepts: [classic-protocol, multicast, write-builtins, desync, temp-entity, validation]
---

The first classic session under `lan` dropped every client a second after the first rocket
exploded, each one reporting "unknown message id" at the end of a frame. The cause was the test
mod's explosion: it wrote `TE_EXPLOSION` and three coordinates to the multicast buffer without
`svc_temp_entity` in front. The bytes went straight into each client's datagram, and the client
read them as other messages until it reached an id it did not know. A classic reader cannot
resynchronise after that, so the rest of the packet was lost.

Two fixes followed.

- **The mod writes its svc byte.** The test mod now writes `svc_temp_entity` before the kind.
- **The server checks the bytes.** Before a mod's Write bytes (multicast, all, one, unreliable)
  reach a client, the server reads them as whole svc messages under that client's agreed
  extensions. Bytes that do not read are dropped and counted (`ServerCounts::mod_bytes_dropped`;
  the server test holds it at 0). `read_string` now marks the reader bad when the terminator is
  missing. Without that, a stray string ran to the end of the buffer and passed the check.

When a scripted client says "unknown message id", hex-dump the payload from the first byte and
look for the mod's bytes before you look at the codec.
