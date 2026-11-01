---
type: Changed
title: the BSP renderer draws sky, liquids and animated textures from the map reader's marks
---

Every BSP family's reader now marks each texture as sky, as a liquid of a kind (water, slime, lava, teleport) and with its animation's next frame, from what that family stores. The renderer and the light bake read those marks instead of texture names, so a version 38 or 46 map draws its sky, liquids and animated textures through the same code as the classic game's maps. The classic game's maps draw exactly as before.
