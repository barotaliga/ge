---
title: Relief on dark low-resolution textures reads as wet foil until its normals and highlights are tamed
date: 2026-10-31
scope: plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_draw.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [relief, normal-map, parallax, ggx, sphere-light, fresnel, wgsl-size-cap, lab-check]
---

- Baked normals at full strength on dark painted walls look like foil: scale the tangent normal
  (0.8), raise the roughness floor (0.45 + 0.55 r), widen each light's lobe by its size over twice
  its distance, lower Fresnel's grazing end with the roughness, and give the sun no highlight.
- No tangents are stored: the frame comes from the screen derivatives of the texture coordinates;
  flip the geometric normal toward the eye first, and keep the rebaked path on the unflipped one.
- The embedded WGSL's 60000-byte cap counts trailing comments (full-line ones are stripped): move
  them onto their own lines before cutting code.
- Judge it on the real pack at grazing angles beside a torch; the fixture (`se-client
  --game=fixture --before="ce_relief 0"`) is the public before and after.
