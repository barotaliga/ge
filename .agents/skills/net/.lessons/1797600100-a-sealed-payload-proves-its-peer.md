---
title: A sealed payload from a new address proves its peer; move the peer there, never a plain one
date: 2026-11-15
scope: engine/net/node.cpp, engine/net/stress.cpp, engine/lab/labs/net_chaos_lab.cpp
concepts: [rebind, nat-rebinding, sealed, aead, replay-window, stranger, chaos, rebind-peer]
---

A NAT that gives a client a new port makes every later packet a stranger's, because a node finds
its peer by address. On a secure node, try a stranger's sealed payload against each connected
peer's key (after `window_allows` on its counter, on a scratch copy so a failed try leaves the
datagram whole) and move the peer it opens under; then let the payload take the normal connected
path, which checks, opens and marks it again. Run the tries behind `admit_handshake`, so a flood
of fake payloads costs what a flood of requests does. Leave plain nodes alone: their payload's
session key is readable on the wire, so following it would let anyone who saw one packet take the
connection, and the plain digests must not move. In a chaos script, state what a sealed run must
show by setting `keeps_connections` from the base plan's `secure`, so the same script holds the
plain run to "reconnected" and the sealed run to "no drop".
