---
title: A thick cloud is white by its many scatterings, and a cover is a quantile, not a threshold
date: 2026-10-23
scope: engine/render/clouds.wgsl
concepts: [clouds, multiple-scattering, reduced-extinction, beer, cover, quantile, cloud-cut]
---

With Beer's law and a single scattering lobe a heap of 0.04/m drew as a grey lens: the light
toward the sun crosses an optical depth of 20, so only the sky's light was left on it. A cloud
absorbs almost nothing, and its light scattered on goes mostly forward, so it dies only at the
reduced extinction sigma (1 - g) (g about 0.85); a second lobe at that extinction, weighted for
the orders past the first (3 here), made the sunlit side as bright as the snow and left the base
grey, as it should be. Judge it from a view with the sun behind the camera: from the sunward side
a heap IS grey with a bright rim, and that looked like the bug when it was not.

A cover set as a threshold on the noise gives a share that depends on the noise's histogram;
cutting at the noise's own quantile (`cloud_cut` off the bytes' histogram) makes the cover the
share of the sky. Measure the share over whole tiles of the noise, not over one map: a 2 km map
under 1.5 km cells read 0.05 for a dealt 0.30 because it held two cells.
