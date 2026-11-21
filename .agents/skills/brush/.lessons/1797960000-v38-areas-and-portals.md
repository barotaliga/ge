---
title: Version 38 areas live in two lumps beside the leaves; flood them in the brush plugin
date: 2026-11-21
scope: plugins/brush/areas.cpp, plugins/brush/read_v38.cpp
concepts: [area, area-portal, flood-areas, v38, leaf-area]
---

A version 38 leaf's second 16-bit field is its area; lump 17 holds each area's run of portal
records (count, first) and lump 18 the records (portal number, area across), a portal listed
from both sides and record 0 a dummy (base1: 3 areas, 3 records, portal 1 joining areas 1 and 2).
`flood_areas` is the one flood every caller uses (the rules server through `map_areas_connected`,
a game library through `areas_connected`); keep portal state as flags in the caller, not in the
`Map`, which is shared and const. A map with no area data answers "joined" everywhere, so
families 29 and 46 need nothing.
