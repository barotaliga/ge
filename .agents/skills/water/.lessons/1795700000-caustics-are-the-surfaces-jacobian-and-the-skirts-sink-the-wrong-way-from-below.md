---
title: Caustics are the surface's own Jacobian, and a clipmap's sunk skirts hang in the view from below
date: 2026-11-02
scope: engine/render/wgsl/water_body.wgsl, engine/render/water.wgsl, engine/render/underwater.wgsl
concepts: [water, caustics, shafts, god-rays, jacobian, clipmap, skirt, underwater, lab]
---

A caustic pattern drawn as a texture or cells never matches the waves. The light under a refracting
surface is 1 / |det(I + s H)|, with H the height's curvature summed over the components the surface
already draws and s about a quarter of the way down the sun's refracted ray. Leave out waves shorter
than the sun's disc blurred over the depth, and the net coarsens with depth by itself. Light shafts
are the same function marched along the eye's ray. Read a softer net for them (four times the blur):
the bed's sharp one turns the march into grain, not rays. Cap the forward-scattering peak, and limit
how far the negative part can darken, or looking at the sun punches a black hole.

A clipmap ring sinks the vertices a finer ring covers so they hide under the ground. Seen from under
the water those skirts hang between the eye and the bed and cover it: raise them for the underside
(`kBelow`) and let it write depth. A one-column edge measurement (Snell's window) stops at the first
dark fragment once waves break the rim; measure by the mean round rings. And a lab's smoke size can
pass while the full size fails, so run the subject's lab at its own size before calling it green.
