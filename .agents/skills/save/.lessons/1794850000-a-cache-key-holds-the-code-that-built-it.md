---
title: A content cache's key holds the stamp of the code that built it, and a round trip is checked by a run
date: 2026-11-14
scope: engine/save/cache.hpp, cmake/SourceStamp.cmake, plugins/mountain/mapgen/level_io.hpp, games/fall_line/sim/map_cache.cpp
concepts: [content-cache, cache-key, source-stamp, describe, round-trip, digest]
---

The generator's version number does not move when its code is fixed, so a cache keyed on the
setup alone serves a stale map after any generator change. Put a stamp of the code in the key
(`ge_source_stamp` hashes the source directories at build time, rebuilt when a file changes). A
map's digest reads only what a skier meets, so a field left out of a describe can pass a digest
check: also write the read value again and compare bytes, and step a run on the cached map beside
one on the generated map to the same state digests (tests/fall_line_map_cache_test.cpp). A free
ride's map is tens of MB as it lies (three float heightfields), so time the cache on its first
step, not on the read alone.
