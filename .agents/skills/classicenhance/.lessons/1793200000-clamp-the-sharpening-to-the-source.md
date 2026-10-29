---
title: Back-projection and sharpening ring at hard edges unless clamped to the source's 2 × 2 range
date: 2026-10-28
scope: plugins/classicenhance/texture.cpp
concepts: [upscale, back-projection, sharpen, halo, clamp, psnr, normal, sobel]
---

A kernel-regression upscale alone measured blurrier than Catmull-Rom on the fixtures' mip PSNR;
one round of back-projection and a light unsharp mask closed the gap, but both overshoot at a
palette texture's hard edges (bright on one side, dark on the other) and drew halos down to black.
Clamping every output texel to the min and max of the 2 × 2 source texels under it removes the
halos and costs no measured sharpness. Tune the radii against PSNR of a quarter-size copy
upscaled back, never by eye alone. A Sobel normal needs dividing by 8 and its depth scaling by the
upscale factor, or every map reads as deep relief.
