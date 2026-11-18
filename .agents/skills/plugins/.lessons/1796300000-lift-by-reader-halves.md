---
title: Lift a game's feature as engine core plus one reader half per file family
date: 2026-11-18
scope: engine/overview
concepts: [plugin, lift, engine, brush, morph, adapter, live-map, digests]
---

The live map lifted cleanly by splitting on what each piece reads: the drawing that needs no file
format (shading, the liquid layer, roofs, the mesh raster) went to `engine/overview`; the call
that reads a map family went beside its reader (`plugins/brush/app/overview_floors`) and the one
that reads a model family beside its own (`plugins/morph/overview_mesh`), each reader linking the
engine module, never the other way. The game kept only what its things mean (item table,
player colours, demo). Move code verbatim first and prove it with the lab digests unchanged
(`labs <id> --smoke --check-digests=tests/digests`); change behaviour in a later PR.
