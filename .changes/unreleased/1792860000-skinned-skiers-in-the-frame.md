---
type: Changed
title: Fall Line's racers drawn as the skinned skier model
---

Fall Line's frame draws every racer as the skier model, skinned on the GPU, in place of the placeholder figure. The model is compiled into the game as bytes (`ge_embed_binary`, `cmake/EmbedBinary.cmake`), each racer has his own mesh and poser, and the `skinning` pass runs before the culls so the shadows, the prepass and the colour pass all read the posed mesh. `FrameScene` gains `dt` and `settle`, so the figures' lags run between frames. `skier::pose_reach` is the fixed box every pose stays inside.
