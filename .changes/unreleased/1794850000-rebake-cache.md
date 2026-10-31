---
type: Added
title: Classic maps' light rebaked into the enhance cache
---

Classic enhance's lightmap stage now works out every map's light again: 0.25 m luxels, light bounced twice off the walls and tinted by their colours, and the light's direction at every luxel. It writes the result to `lightmaps/<map>.gerb`. The shareware pack's 21 maps take about four minutes on four cores. Under the modern look, `ce_rebake` adds this bounce under the live torches and lamps. Before, the look approximated the bounce with a share of the pack's own lightmaps. The rebake file (GERB version 2) also carries the bounce on its own, and version 1 files still read. A map's light entities are now read in one place (`brush/lights.hpp`), shared by the renderer and the bake.
