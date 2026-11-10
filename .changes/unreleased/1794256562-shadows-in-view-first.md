---
type: Fixed
title: A brush map's shadows go to the lights in view first
---

The modern look's 32 shadowed lights were the nearest by distance less reach, so a map's big lights behind walls could take half of them and leave the room in view lit flat, its shadows popping in a few steps later (dm4's doorway into the lava room: 14 of the 30 lights in view had none). The lights the eye's potentially visible set holds now take the slots first, nearest first, then the rest.
