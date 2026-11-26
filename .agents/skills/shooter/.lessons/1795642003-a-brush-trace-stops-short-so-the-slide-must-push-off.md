---
title: A trace of brushes stops 1/32 short, so a slide over them must push off a plane met twice
date: 2026-11-25
scope: plugins/shooter/slide.cpp, plugins/shooter/ladder.hpp, games/shooters_edge/labs/se_remix_ladder_lab.cpp
concepts: [slide-move, same-plane, brushes, version-38, ladder, remix, stuck]
---

Quake's move on q2dm3 climbed none of its ladders even after it took hold of them: on a sloped
ladder face (its normal off the axes) the slide met the same plane again at fraction 0, counted
it twice, crossed the two equal planes to nothing and stopped the player dead. A hull trace never
does this; a brush trace stops 1/32 short of every plane, so it does. Quake II's slide pushes
the velocity off a plane met twice by its normal, and the classic slide now does the same on a
map of brushes only (`Mover::brush_world`), so Quake's own maps keep their bits. When the classic
move misbehaves on a version 38 map but Quake II's does not, print each command's slide
(fraction, startsolid, plane) before suspecting the rule: the geometry's epsilon is the usual
cause. `se-remix-ladder` with `--q2-map` lists every ladder with both moves' climbs side by side.
