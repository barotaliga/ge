---
title: A small particle needs a pixel's width, a glint a flare far past white, and a pool's live count before its rates
date: 2026-10-22
scope: engine/render/particles_sim.wgsl, engine/render/precip.wgsl, plugins/mountain/app/snow.wgsl, engine/render/bloom.hpp
concepts: [particles, spray, snowfall, sub-pixel, glitter, glints, reactive, bloom, mip-chain]
---

- What a ski throws is born under the snow wherever the skis sink: kill a particle at the ground
  only when it falls (`v.y < 0`). Check a pool's live count beside its births before tuning rates.
- A 1-2 cm flake is under a pixel past a few metres: measure the quad's pixel width in the vertex
  stage, widen it to about 1.5 px and divide its alpha by the same factor, and thin a streak by
  the root of its length. Check it sunlit too; in shade flakes read as grey specks.
- Sunlit snow sits near the tone curve's shoulder, so a glint needs about 20 times the flare
  that first seemed right. Measure glitter by contrast against the eight neighbours (the `snow`
  lab's `glints`), and mark it in the reactive mask or TAA-U averages it away.
- A chain whose length follows the target's size carries its own count: the bloom writes alpha 1
  a level and the top's alpha is the divisor.
