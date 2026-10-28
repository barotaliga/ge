---
title: A glow hundreds of times the white bands into rainbows unless it is whitened as it brightens
date: 2026-10-24
scope: engine/render/clouds.wgsl, engine/render/wgsl/frame.wgsl
concepts: [clouds, corona, aureole, diffraction, halo, moon, tonemap, exposure, veil]
---

The diffraction lobe round the sun (half of a large particle's extinction, peaking at
(k a)²/4π per steradian, about 10⁴ for a crystal) is physically hundreds of times the picture's
white for several degrees, and its coloured rings are too. Added as it is, each channel clips on
its own in the tone map and the dark rings of red, green and blue, which fall at different
radii, band the glare into a rainbow ten degrees wide that reads as a lens fault. Ease the
sun's glow toward a few whites and blend its colour to white as it brightens, so the middle is
a white glare and the rings show pastel only where they are about as bright as the cloud. The
moon's disc is already drawn compressed (`as_seen`), so its glow must be compressed the same
way (its ratio to the disc to the 0.35 power) or it outshines the disc it surrounds. Two more
traps: a thin layer's colour at sunset comes from light through its edges, so lighting every
sheet only through its top (the sun's height's sine) took the pink off the cirrus; make that a
kind's `deck` knob. And look at the sun from the lab's `veil` view: the `clouds` view keeps the
sun behind the eye, where none of this shows.
