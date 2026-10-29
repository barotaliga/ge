---
title: A lamp at night burns the picture out until the exposure knows the lamps are there
date: 2026-10-28
scope: engine/render/lights.hpp, engine/render/wgsl/frame.wgsl
concepts: [lights, exposure, night, adaptation, lamp-lux, units, candela]
---

The exposure is read off the sky's light, so on a moonless night it sets the gain for 0.0015 lx
on snow. The first lit courtyard in the `gpu-lights` lab put 35 cd torches and a 900 cd street lamp
into that gain and every lit surface went white: the lamps were right in their units, the eye was
wrong. `FrameView::lamp_lux` adds the lamps' light on a white face to the sky's in
`white_luminance`; take it as the geometric mean of a few points of what the eye looks at (the
lit pools and the dark between them), not the arithmetic mean, which the brightest pool
dominates and leaves everything else black. Check any night scene with lamps at the eye's
adaptation, and the unlit comparison at the same `lamp_lux`, or "the lamps brighten it" measures
the exposure, not the lamps.
