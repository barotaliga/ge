---
title: Listen on IPv6 with a second socket inside the same transport, not a second transport
date: 2026-11-23
scope: engine/net/transport.cpp
concepts: [ipv6, udp, transport, net-ip6, net-enable-ipv6, dual-stack, net-restart]
---

A game reads one `Transport`; giving it a second transport for IPv6 would mean every game's
receive loop and every reply learning about two. `open_udp6_beside` keeps an IPv6-only socket
(`udp6`) inside the IPv4 UDP transport: a send to an IPv6 peer leaves by it, a receive reads the
IPv4 socket first and then it, and `close_transport` closes both, so `net-restart` must open it
again after reopening the IPv4 one. The container has no IPv6 (socket() fails with errno 97), so
the round-trip test skips here and runs on the Mac and Windows rows.
