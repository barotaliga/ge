---
title: A soft shadow's taps let weather through a roof, and relief on dark classic textures reads as foil until tamed
date: 2026-10-31
scope: engine/render/wgsl/fall_shelter.wgsl, engine/render/precip.wgsl, plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_draw.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [shelter, precip, pcss, roof, indoor, depth-map, march, acne, bias, relief, normal-map, parallax, ggx, fresnel]
---

- The fall's shelter map filtered like a contact-hardening shadow reached out from under a roof to
  the deeper open ground and read it as open, so snow settled in rooms; the lab's CPU probes never
  looked at the GPU's picture. `fall_share` marches each way of the cone up toward the sky (seven
  steps, 0.12-10 m) and shuts a way at its first covered step. The bias scales with the slant:
  `0.03 + 1.5 * texel * (1 + run)` m.
- Baked normals on dark painted walls look like foil: scale the tangent normal (0.8), raise the
  roughness floor (0.45 + 0.55 r), widen each light's lobe by its size over twice its distance,
  lower Fresnel's grazing end with roughness, give the sun no highlight. With no stored tangents,
  the frame comes from screen derivatives of the texture coordinates: flip the geometric normal
  toward the eye first. Judge on the real pack at grazing angles beside a torch; the public before
  and after is `se-client --game=fixture --before="ce_relief 0"`.
