---
title: A text packet log is one more transport hook beside the capture and the tap
date: 2026-11-22
scope: engine/net/transport.cpp
concepts: [packet-log, transport, capture, tap, hex-dump, net-log]
---

Everything a server sends or receives passes `send_now` and `receive_now` in transport.cpp, where
the capture and the tap already hook in. A third pointer (`Transport::log`) gives a text log of
every datagram without touching a game: the dedicated process points its sockets at it each frame
(so a `net_restart` that reopens the socket keeps logging) and clears it when the log is off.
Log after a send succeeded, never before, so a failed send leaves no entry.

A change to `engine/net` rebuilds most of the tree (every game links it): build only the test
target first, and expect a full sesv build to take minutes.
