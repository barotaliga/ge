---
title: A surface's glow is tuned in the light it is seen in, and dusk clips it soonest
date: 2026-10-29
scope: plugins/mountain/app/building_look.cpp
concepts: [emissive, emissive_nits, exposure, tone-curve, dusk, night, windows, glow]
---

The post pass sets its exposure from the sky's light. At dusk that light is low, so the exposure
is high, and a lit window of 15 or 40 cd/m² clipped to flat white in the 18:30 view. Its warm
colour was gone, and the two values gave the same picture. 5 cd/m² reads warm at dusk and still
as points of light at 22:00. When tuning `Surface::emissive_nits`, look at the brightest dark view
(dusk, where the exposure is highest) and lower the value until its colour survives. A deep-night
view looks fine across a wide range and decides nothing.
