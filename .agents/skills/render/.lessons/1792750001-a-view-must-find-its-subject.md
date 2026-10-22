---
title: A view meant to show an effect must find a place the effect can show
date: 2026-10-22
scope: games/fall_line/app/views.cpp
concepts: [views, lab, shadows, sun, wood, picture, low-sun]
---

The trees' shadows did not show in any of the `world` lab's first views: the piste lies in the
mountain's own shade at the map's afternoon hour, and the sunlit woods were past the far
cascade. A view whose job is to show an effect has to search for it: `wood` picks a tree on a
wood's sunward edge whose ground faces the sun and sees it past the mountain (a CPU march toward
the sun to about 10 km, as far as the horizon maps reach), and stands across the sun from it.
When no wood sees the sun (seed 38 at 15.8 h) it says so in the caption and the review runs at
`--hour=12`. Check a new effect's view at the map's own hour as well as a chosen one.
