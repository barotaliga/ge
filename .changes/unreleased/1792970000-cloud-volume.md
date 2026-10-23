---
type: Added
title: The cloud volume
---

`render/clouds.hpp` draws up to three cloud layers (`FrameView::clouds`, render/view.hpp's `Clouds`): one tileable 64³ noise made on the CPU and bound in the frame's group 0, a cover that is a true share of the sky (`cloud_cut`), heaps that grow from a flat base and sheets that thicken about their middle, lit by the key light through the air, the layers above and their own cloud (with the many orders of scattering a thick cloud's whiteness takes), hazed and melting into the sky far off, in a "clouds" pass after the sky. Every layer casts its shadow: `sun_light` in `wgsl/frame.wgsl` now takes the place's `xz`, and each place gets its own share of the sun. `plugins/mountain/app/cloud_field.hpp` stands the mountain's broken skies as layers (fair heaps, a flurry's towers under a rippled sheet, high streaks over a veil), dealt off the map's seed and carried by the wind aloft, and `weather_look` gives back a broken sky's mean cloud where its layers are drawn. Fall Line's frame draws them, and the `weather` lab measures each layer's cover and its shadow's share and takes `--lift` to show the sky.
