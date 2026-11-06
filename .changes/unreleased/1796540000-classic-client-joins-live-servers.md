---
type: Added
title: Shooter's Edge's classic client joins existing servers
---

The classic client now signs on to existing servers of the classic protocol and plays: its first `prespawn` carries the map's checksum (MD4 over each lump, with the rebuilt released maps reported as the first release's), it reports its player and eyes models, and it obeys the colour, team, skin and name a mod stuffs. The `se-interop` lab joins a live server with `--server=<host:port>` (`--basedir` for the map, `--record` to keep the session as a client demo).
