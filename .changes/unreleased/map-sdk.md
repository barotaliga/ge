---
type: Added
title: Maps written as Python scripts, any built map imported, and .map text built
---

The map SDK (`tools/mapsdk`, standard library Python) lets an agent write a map as a script: rooms, halls, pits, stairs, liquids, items, starts and the map's intent, then whole maps moved, mirrored, turned, cut out (sealed) and merged, saved as the map editors' `.map` text and an `.intent`. `ge map import` turns any built map (versions 29, BSP2, 38 and 46) back into `.map` text, and `ge map export` builds `.map` text or a plan into a version 29 map, refusing a leak with its place. `make mapsdk-check` runs the SDK's tests; `tools/mapsdk/examples/crossfire.py` is a small duel map built with it.
