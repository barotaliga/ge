---
title: A mission pack is enhanced laid over its base game, keyed by every pack in order
date: 2026-11-24
scope: plugins/classicenhance/source.cpp, plugins/classic/content/files.cpp
concepts: [mission-pack, cache-key, set-key, content-cache, palette, laid-over-base]
---

A mission pack's pak0 has no palette, so the pipeline refused it ("the pack has no palette").
`open_source` on the pack's folder now finds the base folder beside it (any case) and lays the
pack's packs after the base game's in `Source::later`: the key is `set_key` of the base's digests
then the pack's. The game's side is `content_cache` (classic/content/files.hpp): the same packs in
the same order (`content_packs_with_mission`), falling back to the base game's cache. A change to
one side of that key without the other makes the window silently draw the base game's textures;
`tests/classic_content_files_test.cpp` and `tests/classicenhance_source_test.cpp` hold both.
