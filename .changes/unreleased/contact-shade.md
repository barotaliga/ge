---
type: Added
title: Contact shade under classic enhance (ce_ao)
---

With classic enhance's modern look, the corners, the seams where walls meet floors, steps, door frames and the floor under the monsters and items darken where little light reaches them, in Quake and Quake II alike. `ce_ao` (CONTACT SHADE on the LIGHT AND SHADOW page) turns it off; `ce_ao_strength` (SHADE LEVEL, 0.8) sets how dark, `ce_ao_radius` (48 units) how far a wall still shades the floor beside it. The engine's ambient occlusion can now be applied after a frame's colour is drawn (`encode_ao_apply`), for a renderer with no depth prepass.
