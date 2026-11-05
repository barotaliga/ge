---
type: Added
title: An explosion's flash lights the world and its scorch is pressed
---

`flash_lights` (`engine/render/explosions.hpp`) hands every live blast's flash to the local lights each frame, white-hot then orange, its reach following its light, so a blast lights the ground and the walls round it by day and at night. `engine/render/marks.hpp` presses a blast's scorch with the decals and draws any pool of decals (scorches, bullet holes, blood) as the beams pass's burns; Shooter's Edge's marks now use it. The `gpu-explosion` lab shows both in its yard.
