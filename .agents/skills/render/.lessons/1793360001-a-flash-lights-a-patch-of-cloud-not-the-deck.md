---
title: A lightning flash lights a patch of its cloud, not the whole deck
date: 2026-10-30
scope: engine/render/wgsl/flash.wgsl, engine/render/clouds.wgsl
concepts: [lightning, flash, cloud, diffusion, spread, reference-photos]
---

A diffusion law alone (light falling as 1 / (1 + 0.15 tau) through the cloud) let a strike light
the whole storm deck white, because nothing stopped the light spreading sideways. A real cloud
loses the light through its faces as it spreads, so the glow falls off across the cloud over about
its depth. Night photographs of strikes are the check: the channel is the brightest thing in the
frame and the cloud glows only in a band a kilometre or two across where the channel leaves the
base. `flash_spread` (exp(-r / 1200 m)) and one order of scattering for a flash's light, not the
sun's three, match them. Compare a light against reference photographs before tuning its gain.
