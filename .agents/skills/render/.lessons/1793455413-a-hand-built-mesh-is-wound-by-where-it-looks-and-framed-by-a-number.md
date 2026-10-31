---
title: A hand-built face is wound by where it looks, and a held or hidden subject is framed by a number
date: 2026-10-23
scope: plugins/mountain/app/building_look.cpp, games/shooters_edge/view/weapons.cpp, engine/render/instances.wgsl, games/fall_line/labs/gpu_camera_world_lab.cpp
concepts: [mesh, winding, add-triangle, roof, soffit, viewmodel, mount, framing, see-through, dither, depth]
---

- `render::add_triangle` takes the normal from the corners' order; `building_look.cpp`'s `face`
  takes the direction the face should look and swaps corners to match. A roof seen from below
  needs its soffit too. Only pictures show a backwards face, so look from below as well.
- One mount rule for a kit of guns frames none: each needs its own held length and forward offset
  (0.30 to 0.44 m, bulky ones furthest). Tune by the placed box's screen share, a row a weapon.
- How much of the skier a dithered occluder fade shows is measured off the depth target: project
  points down his middle with `view_proj` and count each whose depth is no nearer than half a
  metre toward the eye. Keep the fade in the prepass under the LOD dither, so the `equal` colour
  pass follows and shadow casters keep the whole tree.
