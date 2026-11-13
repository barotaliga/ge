---
title: The second model family draws through the first family's path once its pictures are indices
date: 2026-11-13
scope: plugins/morph/read.cpp, games/shooters_edge/client/adapter_parts.cpp
concepts: [md2, second-model-family, skin-paths, pcx, open-model, world-models]
---

The window's world models already draw any `morph::Model` whose skins are palette indices: a
file that names its skins as PCX pictures only needs the adapter to read each picture's indices
into `Model::skins` (`assets::decode_pcx`), and the items, monsters and players stand on the map
with their skins, lit and in both looks, with no change to the draw. The first picture of the
sequel's things came from one reader and twenty lines in `open_model`. The winding is the same
as the first family's (clockwise from outside, swapped once); the `se-sequel-models` lab's
monster and player pictures showed no torn faces. A player's model names no skin: the
adapter gives it the folder's `grunt.pcx` until the player's own choice reaches the window.
