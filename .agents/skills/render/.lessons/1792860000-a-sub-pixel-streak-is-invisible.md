---
title: A sub-pixel streak is invisible; widen it to a pixel and thin it as much
date: 2026-10-22
scope: engine/render/snowfall.wgsl
concepts: [particles, snowfall, sub-pixel, coverage, streak, motion-blur, alpha]
---

The first snowfall lab drew 13000 flakes and showed none: a 1-2 cm flake is under a pixel wide
past a few metres, so the rasteriser covered almost no pixel centres, and thinning each streak
by its whole length (a fair share of the shutter) left a storm's driven flakes at a few percent
alpha. Measure a quad's width in pixels in the vertex stage (project its centre and one metre
across), widen it to about 1.5 px and divide its alpha by the same factor so it keeps its light,
and thin a streak by the root of its length. Check a picture effect against a sunlit view too:
in a mountain's shadow, flakes lit by the sky alone read as grey specks against a clear sky.
