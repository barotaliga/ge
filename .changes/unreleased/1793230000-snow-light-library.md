---
type: Added
title: The snow light model as an engine WGSL library
---

`render/wgsl/snow_light.wgsl` gives any material that draws snow the engine's light model: `snow_relief` (the micro-relief as a slope) and `snow_light` (Lambert and the sky, the wrap's blue terminator, the glare, the wet sheen and the glitter), plus the noise, relief and glint blocks for a material's own additions. The mountain's snow material now uses it, and its pictures are unchanged bit for bit.
