---
title: Ask the piste about a tree only near the course, never for every tree of a ski area
date: 2026-10-21
scope: plugins/mountain/analysis/race.cpp
concepts: [analyze-race, nearest-within, trees, slalom, performance]
---

A ski area carries 40 000–60 000 trees, and `nearest_track_point` walks its hash's rings
outward until it finds the piste, so a tree a kilometre off costs hundreds of cells. The
slalom's analysis asked it of every tree and took 6 s a course; `nearest_within` with the
widest piste's half-width plus the cleared band answers a far tree with Infinity at once, and
the same check took a few milliseconds. Any check or lab that asks where a tree stands against
a course bounds the question by how far it cares.
