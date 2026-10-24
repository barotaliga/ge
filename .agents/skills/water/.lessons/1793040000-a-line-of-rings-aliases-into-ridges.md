---
title: A line of impulse rings is Kelvin's wake only if its short diverging waves are kept
date: 2026-10-24
scope: engine/water/wake.cpp
concepts: [water, wake, kelvin, rings, aliasing, diverging-waves, measurement, lab]
---

A hull drawn as a line of rings needs the rings closer than a third of the shortest wave kept, or
the sum aliases into ridges along the track. The first fix for that kept no wave shorter than
half the hull: the arm's angle measured right, but only the transverse waves survived and the
wake read as arcs across the track with nothing along its arms (the owner caught it from the
picture). What a wake looks like is mostly its DIVERGING waves, from two thirds of the transverse
wave at the arms to ever shorter toward the track: keep down to an eighth of the transverse wave,
space the rings by distance (a third of that), damp the very short ones with the source's own
spectrum exp(-k^2 b^2 / 4) rather than a floor, and fade in each ring's first half turn behind
its front, where the far-field formula is wrong. Measure the arm at the wake's OUTER EDGE (the
last point where the envelope is half the line's highest), not at its highest wave, which sits
on a diverging crest inside the arm. Check the plan against Kelvin's crest lines drawn over it,
not only against the angle: a number can pass while the picture is wrong.
