---
type: Fixed
title: A brush map's shadowed lights are the ones that light the view, 48 of them
---

The modern look shadowed only 32 lights, and from a long corridor that sees many rooms the lights ahead could miss out and shine through what stands before them. In start, the NORMAL hall's slipgate frame threw its shadow on the wall behind it (the map's own lightmap has it) only once the player walked into the hall. The shadow slots now go first to the lights in view whose reach comes into the picture, and all 48 kept cubes are used, so the shadow is there from the corridor too.
