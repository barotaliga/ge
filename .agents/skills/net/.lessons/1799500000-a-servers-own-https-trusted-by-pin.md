---
title: A game server's own HTTPS needs no public certificate: pin the one it makes
date: 2026-11-23
scope: engine/server/web_files.cpp
concepts: [web-files, https, certificate-pin, self-signed, sv-http-port, fast-downloads, rate-cap, fair-share]
---

A dedicated server rarely has a host name a public authority would sign, and an IP address in a
certificate fails the name check, so "HTTPS with real verification" by the system's roots would
leave most servers on plain HTTP. Instead the server makes its own certificate at start and tells
its clients the certificate's SHA-256 beside the address (`*dlpin`, ` dlpin=`); the client sets
`TlsOptions::pin` and checks the peer certificate's DER after the handshake. That is exactly as
trusted as the UDP connection the address came over. Two traps met on the way: a token-bucket cap
with a quarter-second burst let a whole 632 kB map through before the cap applied (now a
sixteenth); and a client that probes packed forms (`.xz`, `.zst`, ...) first costs four TLS
handshakes per connection against a server that keeps files as they are, so a pinned address
skips the probe. Time both ways with `se-fastdl --map=<a .bsp>`.
