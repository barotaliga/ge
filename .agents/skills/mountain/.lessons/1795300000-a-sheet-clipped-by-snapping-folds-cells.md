---
title: Clipping a grid to a shore by snapping corners folds cells; drop the folded triangles
date: 2026-11-01
scope: plugins/mountain/app/ice_look.cpp
concepts: [ice-sheet, clip, shore, moat, mesh, normals, thaw, shade]
---

The ice sheet is a grid clipped to the lake's shore by drawing every corner outside in to the
nearest point of the shore (or of the moat's line inside it). Two corners of one cell can land on
the same point, or cross, and the triangle then has no area or faces down: the mesh's normals go
bad and the unit test's "faces up" check catches it. Keep only triangles that still face up with
some area (`faces_up`), and measure the sheet by its triangles' areas, never by its cell count.
A moat of open water round a thawing sheet reads only faintly while the cirque is in shade: the
water's sky reflection and the slush's diffuse sky light come out close. Look at it in the sun
before judging the tone.
