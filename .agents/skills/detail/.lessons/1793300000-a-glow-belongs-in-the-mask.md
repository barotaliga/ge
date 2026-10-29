---
title: A decal's glow needs a pattern of its own in the mask, not the cover
date: 2026-10-28
scope: engine/detail/decals.cpp
concepts: [decal, scorch, ember, emissive, mask, atlas]
---

The first ember glow on a scorch weighted the look's emission by the mask's cover. Every burnt
pixel glowed, and the fresh scorch read as one orange blob rather than embers. Embers sit in
patches of the burnt core and along the blast's tongues, so the mask gained an `ember` channel
(baked into the atlas's blue) and the look's glow multiplies it. When a mark's light, wetness or
any other property needs a different pattern from where it covers, give that property its own
mask channel. The GPU pass and the CPU labs then read the same pattern. Also: a lab's event
fixed at age 0 now glows, so a scene meant to show the aftermath (the fire-fight hall) ages its
scorches past `ember_s`.
