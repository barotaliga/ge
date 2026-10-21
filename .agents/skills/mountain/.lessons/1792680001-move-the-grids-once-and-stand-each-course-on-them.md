---
title: Move a ski area's grids out once and stand every course up on them in place
date: 2026-10-21
scope: plugins/mountain/mapgen/resort_level.cpp
concepts: [resort-level, set-course, analyze-course, memory, course-map]
---

A ski area's ground, packed field and crust are 2001² floats each (16 MB apiece at 2 m over
4 km), and the generator holds every course of an area to its analysis before it races one. A
`Level` per course copies them each time. `resort_base` moves them out of the attempt once, and
`set_course` rewrites only what a course owns (the track, the gates, the start line, the
kickers, cliffs and drifts, the day, the grade); everything a course reads it samples from the
base map, never from the attempt, whose grids are gone. A drift down a run the course skis only
part of is cut to the stretch skied, and the analysis reads a drift's fade 4 m wider than the
rule's: the grid reads a cell either side, and a course's arcs are its runs' eased by a few metres.
