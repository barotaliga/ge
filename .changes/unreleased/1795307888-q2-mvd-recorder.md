---
type: Added
title: Quake II's server records multiview demos and streams them live to relays
---

Quake II's server gains `mvdrecord` and `mvdstop` (a `.mvd2` of every player, gzipped with `-z`, with its size, time and level-change limits) and, with `sv_mvd_enable 2`, serves the same stream over TCP on the game port to relays: the hello and its access lists (`addgtvhost`, `addgtvban`, with hits and the last hit), `sv_mvd_password`, deflate, suspension when no one plays and the TCP viewers in `status`. The classic layer gains an address list with masks and comments any modern classic server can mount, and `engine/core/deflate.hpp` a streaming deflater (zlib and gzip, sync flush).
