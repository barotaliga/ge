---
title: A faint part beside a bright one is drawn as the eye sees it, and a close-up is judged under a clear sky
date: 2026-10-23
scope: engine/render/sky.wgsl
concepts: [moon, earthshine, eclipse, crescent, exposure, tone, close-up, lab-check]
---

The moon's earthshine is a few thousandths of its lit limb and an eclipse's umbra a thousandth
of the penumbra: drawn in linear light, both vanish on any display. Drawing each faint part as
its ratio to the disc's brightest point to a power (0.35 for the earthshine, 0.5 for the shadow)
keeps them visible and in order. Cap the faint part at what it would be on its own, or a
twilight's compression lifts it to the sky's brightness. A disc close-up in a lab was first
"failing" because the map's cloud hid it, then because the check sampled a crescent's centre,
which is its dark side. Draw close-ups under a clear sky and measure a disc's brightest point,
not its middle.
