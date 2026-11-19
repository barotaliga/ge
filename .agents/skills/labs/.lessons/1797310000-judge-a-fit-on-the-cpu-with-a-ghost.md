---
title: Judge how one model sits on another on the CPU, orthographic, with the old part as a ghost
date: 2026-11-19
scope: games/shooters_edge/labs/se_vweps_fit_lab.cpp
concepts: [fit, close-up, orthographic, ghost, wireframe, even-light, software-raster, vwep]
---

A model placed on another (a weapon in a figure's hands) cannot be judged in the map's light:
the dark corners hide the grip and the perspective hides depth. A CPU panel, orthographic (so
the skin's coordinates interpolate without perspective correction), lit by a soft key and a
back fill, with flat colours, the contact region marked (body vertices within 2.5 units of the
part they hold) and the part being replaced drawn as a wire ghost, showed in one sheet what
three GPU passes had not: the carried weapons were 60% of the gun's length and set back from
the hands. Draw the side, above and in front; the top view caught weapons poking into the body.
