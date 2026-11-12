---
title: A lab over loopback TCP keeps one digest when each client's connect is up before the server looks
date: 2026-11-12
scope: games/shooters_edge/labs/se_tv_lab.cpp, games/shooters_edge/net/classic_qtv.cpp
concepts: [loopback, tcp, determinism, digest, lab, accept, stream-port, relay]
---

A sim lab may use real loopback sockets beside the simulated network and still digest the same
every run, if nothing it measures depends on when the OS delivers. Two rules did it for `se-tv`:
pump a new client's `tcp_connect` until it is no longer `connecting` and send its request at once,
so the server's next `tcp_accept` and read find it on the same frame every run (loopback delivers
inside the send call); and count the bytes the SERVER made for each peer (sent, queued and not yet
flushed) rather than what the peer has read so far. Read the peer's bytes only after the close,
drained to the end.
