---
title: A coloured dust has to reach the eye's height, and be coarse, to colour a sky
date: 2026-10-24
scope: engine/render/sky_style.cpp, engine/render/wgsl/atmosphere.wgsl
concepts: [sky-type, dust, mie, scale-height, asymmetry, alien-sky, exposure, tuning]
---

Scaling the Earth's haze up 50 times with a blue-drinking absorption gave a grey sky round a
white glow and white snow, not a desert's butterscotch: the haze's scale height is 1.2 km and
a mountain eye stands 2 to 3 km up, so almost none of it is above the snow, and its asymmetry
of 0.8 throws the light it does scatter forward into a glow round the sun. Two knobs fixed it:
the dust's scale height (10 km for a desert, near the air's own 8) and a lower asymmetry
(0.55 to 0.65), after which a dust of 8 to 20 times the Earth's coloured the sky, the snow and
the clouds alike. The exposure keys to the sunlit white, so a type cannot be made "dim": it
reads as its colour, and a dark air is a strong absorption in every channel, not a weak light.
Tune a type in the `world` lab's `sky` view under `sv_weather clear` at the map's hour and at
noon, since the low sun's long path hides what the air does overhead.
