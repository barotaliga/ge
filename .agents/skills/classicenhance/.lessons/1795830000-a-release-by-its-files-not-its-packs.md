---
title: Know a release by its files; a pack that looks like a repack may be another release
date: 2026-11-02
scope: plugins/classicenhance/known_sets.hpp
concepts: [known-set, repack, single-pack, remaster, manifest, pk3, verify]
---

A player's one big `pak0.pak` is not always the full game repackaged. The one that prompted the
known sets was the 2021 remaster's: it holds every path of the full game but default.cfg, yet 265
of them differ (every map rebuilt, the sounds remade, the programs). Compare a candidate's files
with the original packs' before deciding what it is (`ge tool classicenhance -- manifest` on
both, then diff the rows), and give it its own set rather than loosening the match. A set's
matcher ignores files it does not list, so a repack with extras still matches; never let it pass
a changed file. The startup script's file name trips `make agents-check`'s clean-slate scan, so
it is listed with a null path and matched by its size and digest at any path.
