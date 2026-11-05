---
type: Added
title: Models' shadows in a brush world
---

`plugins/brush/app` draws the moving models a game hands over (`set_shadow_casters`, the first-person player included though never drawn) into a cube atlas of their own for the lights that light them most, soft or sharp by each light's size (PCSS), with capsules standing in for them under every other light; `se-timedemo` reports the GPU's time by pass.
