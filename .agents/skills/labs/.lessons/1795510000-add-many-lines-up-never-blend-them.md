---
title: Thousands of lines on one picture are added up, never alpha-blended one by one
date: 2026-11-01
scope: games/shooters_edge/labs/se_kills_lab.cpp, engine/lab/raster.hpp
concepts: [density, lines, alpha, heatmap, many-demos, overlay, wireframe]
---

A lab that draws every kill or every rocket of many recordings over a map cannot pick one alpha
for its lines: tuned for 300 they vanish at 3000, tuned for 3000 the first demo shows nothing.
Sample each line a pixel at a time into a count and a colour sum (`Density` in `se_kills_lab.cpp`),
then lay the sum over the picture once: the colour the lines there average, the opacity by the
count's log against the 99th percentile's (never the maximum, which one spawn spot owns). The same
picture then reads at one demo and at a hundred, and mixed weapons show as their mix. Draw the few
lines that matter (the gold shots, arrows numbered as in the table) solid on top of it.
