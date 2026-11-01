---
type: Added
title: The game recorded as it is played, and ge demo pack and unpack
---

Shooter's Edge's console gains `record <name>` and `stop`: the match being played is written to `<game folder>/<name>.mvdz` as it plays, packed with zstd (`demo_pack 0` writes a plain `.mvd`), flushed every 2 s so a crash keeps all but the last two seconds, and played back with `playdemo <name>`. `ge demo pack <recording>` packs a recording the same way (to `<recording>z`, checked to unpack to the same bytes) and `ge demo unpack <file>` writes the one recording inside a packed or archived file (zstd, gzip, xz, tar, zip or 7z) plain.
