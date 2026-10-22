---
title: A glint against white snow needs a flare far past white
date: 2026-10-22
scope: plugins/mountain/app/snow.wgsl
concepts: [glitter, glints, tone-curve, shoulder, contrast, taa, reactive, snow]
---

Sunlit snow already sits near the tone curve's shoulder (about 230 of 255), so a crystal's flare
of a few times white maps to 249 and is lost; the gain that reads at 640x360 was about 20 times
what first seemed right. Measure glitter by contrast against the pixel's neighbours (the `snow`
lab's `glints`: 8 levels over the mean of the eight round it), never by a brightness threshold,
which counts the white snow itself. Mark the flare in the reactive mask, or TAA-U averages a
point that comes and goes with the jitter into nothing.
