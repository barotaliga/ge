---
type: Added
title: A body running into water pushes it ahead
---

A body that goes in moving across the surface, like a running jump, no longer makes a round splash. `entry_of` leans its ring toward where it is going, by its angle off the vertical, so the waves ahead are higher and those behind lower. Its crater is drawn out along the run, and the crown is piled up, taller and flung on ahead (`Ring::lean_x`, `lean_z`, `reach`). The lean still moves no water, and the crater is still sized from the speed down. `ring_height` and `crater_height` now take the point's offset on the plan, not a distance.
