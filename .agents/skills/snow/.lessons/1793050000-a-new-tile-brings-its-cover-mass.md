---
title: A press or a groom that allocates a tile adds its cover to the mass; compare against mass_in
date: 2026-10-24
scope: engine/snow/time.cpp
concepts: [groom, mass, mass-in, tile, ledger, weather, bring-to]
---

The first groomer check compared `mass_units` before and after `groom` and failed by millions of
units: the swath reached tiles no press had allocated, and each new tile is filled from the cover
and counted into `mass_in`. A check that an operation moves no mass must compare the change in
`mass_units` with the change in `mass_in` (the ledger), never the totals alone. On a weathered
layer the same holds for `bring_to`, which adds the weather's mass. Also: a warm day settles and
melts loose powder hard (40 cm of powder at +4 °C for twelve hours is about 6 cm of crust), so a
crust test needs a deep cover to leave room to sink.
