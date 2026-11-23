---
type: Added
title: Your own rockets, nails and grenades fly as you fire, and your jump is heard as you jump
---

On a QuakeWorld server, your own nails, rockets and grenades now leave your gun the moment you fire, instead of appearing a round trip later already well on their way. When the server's own rocket or grenade arrives it takes the place of yours without a jump, and the server's nails take over from yours as they arrive. Your jump is heard as you jump instead of a round trip later. A correction of your predicted place is eased out more finely: the part a single step's timing explains is taken at once, a leftover too small to see is dropped, and the eased place never goes into a wall. New settings: `cl_predict_projectiles` (1 on, the default), `cl_predict_jump` (1 on, the default) and `cl_predict_smoothview` (0 snapped at once, 1 the usual pace and the default, up to 2 slower).
