---
type: Added
title: Lava and slime part round a body wading through them
---

A body moving through lava or slime now leaves a furrow that closes slowly behind it, a bow ahead of it and rings out of its track, as thick as the liquid's viscosity: water 1, slime 2, lava 4 (`render/liquid_wade.hpp`, `LiquidLook::wade`). Lava's crust tears open along the furrow and skins over again; slime's scum is pushed aside. A game hands its bodies to `wade_liquids` each frame; Shooter's Edge does it for every player in lava or slime, behind `ce_wade`. A thing crossing the surface is a plunge (`plunge_liquid`): a ball of lava hurled out heaves the lava, and coming down it craters it and throws a few drops a little way up (`liquid_splash`). Lab: `se-wade`.
