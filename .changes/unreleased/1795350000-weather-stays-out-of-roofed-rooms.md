---
type: Fixed
title: Rain and snow stay out of roofed rooms
---

With the weather on, snow no longer falls and settles inside roofed rooms of a BSP map. The shelter test now marches a cone of ways back up toward the sky through the fall's map (`render/wgsl/fall_shelter.wgsl`), one test for the wet and snowed faces, the flakes and drops and the splashes, so a roof keeps the room under it dry however hard the wind scatters the fall; the open ground stays evenly soaked instead of speckled. The `gpu-bsp-weather` lab checks the GPU's picture of its roofed store.
