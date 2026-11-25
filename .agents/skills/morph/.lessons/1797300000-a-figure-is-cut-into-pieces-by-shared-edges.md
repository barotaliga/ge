---
title: A figure's held gun is a piece of its own, cut out by the triangles' shared vertices
date: 2026-11-19
scope: plugins/morph/carry.cpp, plugins/classic/vweps.cpp
concepts: [pieces, cut-out, rigid-fit, carried, vwep, stand-in, player-figure]
---

Quake's player figure is four separate meshes in one file: the body, the gun it holds,
the axe and the muzzle's flash. Grouping vertices by the triangles that share them
(`morph::pieces`) finds them without names; the largest is the body, and the gun and axe are
told apart by which frames fold them to a point (the axe in the gun frames, the gun slung on
the back in the axe frames). A model to put in the hands then rides the gun piece: a rigid fit
of the gun's points from the frame it is measured in to each frame (`fit_rigid`, Horn's
quaternion) moves it the way the gun moves, deaths included. Mount it on the gun's REAR end
along the gun's long axis, not on the centre: pickups are longer than the held gun, and
centred they poke back through the body.
