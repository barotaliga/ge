---
type: Added
title: A WebTransport endpoint, so a browser page can reach a game's servers
---

The engine can now answer a browser's WebTransport session: the page sends a game's packets as
unreliable datagrams over QUIC, and the endpoint takes them as they come. Its certificate is made
when it starts and trusted by the page through its fingerprint, so no certificate authority is
needed. This is the first part of playing online from the hub's page: next, the forwarder sends
each session's packets on to the game server as ordinary UDP.
