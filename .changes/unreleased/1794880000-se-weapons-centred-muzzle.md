---
type: Changed
title: Shooter's Edge's weapons centred, hands optional, real muzzle fire
---

Shooter's Edge holds its weapon centred under the crosshair by default (`r_viewmodel_center 0` puts it back to the right, now lower), `r_viewmodel_hands 0` hides the hands and arms (`HeldModel::hand`, `HeldDraw::hide_hands`), and every shot fires the engine's muzzle flame, smoke and sparks (`render/muzzle.hpp`'s new `tuned_muzzle` and `scale_muzzle`) in place of a light alone; the labs' stage draws a flame-shaped flash.
