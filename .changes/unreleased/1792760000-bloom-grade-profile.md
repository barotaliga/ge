---
type: Added
title: Bloom, the colour grade's lookup table and the profile lab
---

`render/bloom.hpp` adds the glow round bright light as a half-size mip chain (a 13-tap downsample with a Karis-weighted first level, a tent upsample), mixed into the frame by the post pass without a threshold, so it conserves energy. The post pass reads a 32³ colour grade table (`set_grade`, `identity_grade`) at the tone-mapped, sRGB-encoded colour, and Fall Line bakes its own. `render::float_to_half` packs RGBA16F uploads. Fall Line's `profile` lab reports every pass's GPU ms and the renderer's CPU half over a stretch of the bot's run.
