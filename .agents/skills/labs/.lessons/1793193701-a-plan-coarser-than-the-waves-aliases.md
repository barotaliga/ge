---
title: A plan picture coarser than the waves shades noise; picture them close up instead
date: 2026-10-28
scope: templates/water/labs/, engine/lab/raster.hpp
concepts: [labs, picture, aliasing, water, waves, sample, plan]
---

Every pixel of a CPU-raster plan is one point sample, so a lake drawn at 3 px a metre with its
waves' normal in the shading (waves 0.3 m long, a pixel 0.33 m) came out as speckle that read
as a texture, not as a sea rougher at its lee. Shade by the waves only at 10 px a metre or more
(eight pixels to the shortest wave laid), and show the waves in a close-up shot of their own: a
6 m by 8 m patch at 25 px a metre of the windward and of the lee water side by side made the
fetch's work plain at a glance.
