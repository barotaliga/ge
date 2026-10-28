---
title: A sun tens of thousands of times the white leaks through any rounding of the cloud's transmittance
date: 2026-10-25
scope: engine/render/clouds.wgsl, engine/render/wgsl/frame.wgsl, cmake/WgslEmbed.cmake
concepts: [clouds, transmittance, half-float, quarter-resolution, sun-disc, optical-depth, thickness, glow, moon, rods]
---

The sky draws the sun's disc at up to 60 000 against a white near 1, behind the cloud, so the
disc shows wherever the cloud's transmittance is above about 1e-5. Two roundings let it through
every deck: the march stopping at a transmittance of 0.01 (keep it, but take the rest of the
path as thick as the ray's so far), and the half-size targets holding 1 - transmittance in a
half float, whose steps near 1 are 1/2048 (keep the transmittance itself; a half float holds a
small number finely). Find it by marching the same frame at every pixel and at a quarter: if
only the quarter shows the disc, it is precision. Three more traps from making the clouds'
thickness vary: a kind's `opacity` is the full column's, but a sheet's lens and its eaten edge
leave about 0.6 of it on the ray, so a kind that must hide the sun needs about 1.7 times the
depth that should do it; a glow lit by exp(-tau) toward the light read only part way up the
slab ignores the cloud between, so light it by the ray's own transmittance; and the post pass
keeps the moon's disc for the cones, so a cloud in front of it must be given to the rods in the
cloud pass or it shows as a dark spot. The lab's thickness ladders (`sv_thick 0 .. 1`, `veil`
view, `--day=172` for a high sun) show all of it in one strip.
