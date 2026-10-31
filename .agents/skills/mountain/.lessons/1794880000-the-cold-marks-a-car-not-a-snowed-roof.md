---
title: Frost shows on a parked car and black ice on a glossy road, never on a roof already under snow
date: 2026-10-31
scope: plugins/mountain/app/village_look.cpp
concepts: [frost, black-ice, village-surfaces, set-family-surfaces, freezing-rain]
---

Every building's roof is drawn under its snow, so hoar frost on it changes nothing a player can
see; the night's frost reads on what is dark and bare by day: a parked car's paint and above all
its glass, rimed white. Black ice reads only as a change of SURFACE, not of colour: the
carriageway darker and glossy (`rough` near 0, `spec` 0.5) so it mirrors the sky. Rain onto a road
below 0 C needs the fall to be rain while the air at the road is freezing, which the day's own snow
line never gives (it puts rain only where the wet bulb is over 1 C); a freezing rain is a sky
picked by hand with its snow line high (the warm layer aloft) over a cold day's mean. A look that
changes with the hour is written with `render::set_family_surfaces` in steps (`kColdStep`), never
by remaking the family.
