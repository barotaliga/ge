---
type: Added
title: The lamps' shadows
---

Local lights cast shadows: a light given a `shadow` key is dealt tiles of one depth atlas (a spot one, a point light a cube's six faces) when it is among the lamps that light the eye most, the instance families with caster slots are drawn into them, a tile is redrawn only when its lamp moves or something moves in its view, and `local_lights` reads them through a filtered comparison (`render/lamp_shadows.hpp`).
