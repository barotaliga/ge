---
title: A Python map that leaks: probe the brushes at the gap the compiler names
date: 2026-11-15
scope: tools/mapsdk/mapsdk/builder.py, tools/mapsdk/examples/crossfire.py
concepts: [map-sdk, leak, hall, carve, mirror, merge, python-map]
---

The first SDK maps leaked twice. Two rooms joined by a bare `carve` leave the space between their
walls open to the void: a way between rooms is a `hall` (a room of its own, its ends carved
through both walls). The second leak came from a hall that guessed its axis from its longer side
and so cut along the wall instead of through it: `hall` now takes `along` ("x" or "y") and never
guesses.

To find a leak, do not stare at the picture: the compiler names the entity the flood started
from and the last point it passed, so test points near that place against `m.brushes` (a point
is solid when every side's `n . p - d` is negative) and scan the wall planes there for air. It
takes a minute and points at the brush that was cut wrong.

Names in an intent are one word: `area` and `trick` refuse spaces (the lab's reader stops at
them), and `merge` numbers a clashing name `-2`, `-3`, so a wing merged twice carries `ledge` and
`ledge-2`.
