---
title: Wind a hand-built face by the way it should look, not by its corners' order
date: 2026-10-29
scope: plugins/mountain/app/building_look.cpp
concepts: [mesh, winding, add-triangle, normal, gable, roof, soffit, culling]
---

`render::add_triangle` takes the face's normal from its corners' order, so a gable, a roof plane
or a spire built by hand from points is easy to wind backwards, and then it vanishes under back-face
culling. `building_look.cpp`'s `face` takes a direction the face should look along and swaps two
corners when the cross product points the other way. A roof plane is then one call for its snow
looking up and out, and one for its soffit looking down. A roof over eaves that are seen from
below needs that second face; a single-sided roof shows the sky through the overhang. The test
holds every normal to unit length. Only the pictures show a face wound backwards, so look at the
mesh from below as well as from above.
