---
type: Added
title: The map commands read a BSP map from ge and from a game's player CLI
---

`ge map info|ents|liquids|slice|check` and `secli map info|ents|liquids|slice|check` answer a classic map's family and counts, its entities by class or nearest a point, its pools, a cross-section and every map on a search path, with the same bodies (`engine/toolkit/map.hpp` over the brush plugin's tool) and answers that name the program that ran them. `ge` builds the repo's `brush` executable for them; a game's CLI that links the brush plugin runs the tool in its own process. `ge tool brush -- <command>` still runs the tool bare.
