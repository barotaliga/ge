---
type: Added
title: Texture techniques from references, and replacement by likeness
---

`engine/texgen` gains the techniques a round of study against hand-painted reference textures showed: a `finish` group (`speckle` grain and coloured specks, `shade` light baked from the height, `palette` ramps with an ordered dither), `blocks` (uneven masonry), `studs` (domes, pyramids, diamond plate, holes), `scales`, and rivet keys on panels, with ten new library recipes. `look.hpp` measures a texture's look in numbers. `replace.hpp` swaps a level's painted texture for the most alike library recipe, recoloured to the original and with its full-bright texels kept, as an option a renderer can offer. The `texgen-compare` and `texgen-replace` labs show both, and the `texture-references` skill is the loop.
