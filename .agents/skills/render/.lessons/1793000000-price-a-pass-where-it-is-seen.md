---
title: Price a pass from a view where it fills the frame, and expect a software raster to under-report a quarter's saving
date: 2026-10-23
scope: engine/render/clouds.wgsl
concepts: [quarter-resolution, upsample, lavapipe, price-table, ladder, clouds, timing]
---

The clouds marched at a quarter of the pixels priced the same as every pixel from the chase
camera, because there the sky is a sliver and most of the clouds row's cost is the cloud shadow
every lit surface reads, which the march's resolution does not touch. From the start looking up
into the sky the same change halved the row (72 to 36 ms). On lavapipe any full-screen pass costs
about 13 ms at 1280x720 whatever it does, so the upsample alone eats much of the saving a GPU
keeps; per-pass timestamps there also land on the wrong pass (the particles showed 27 ms only
under a cloudy sky). Judge a resolution change by the whole frame's median from a view that shows
its subject, and say that the GPU's saving is the pixels' ratio, not the CPU raster's.
