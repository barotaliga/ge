---
type: Added
title: The scene mirrored in the wet ground
---

Puddles and the rain's film now mirror the scene, not only the sky: the trees, the masts and the slope stand upside down in a puddle and blurred in a wet surface. Any material can ask for the reflection through `scene_reflection`, which marches the reflected ray through the last frame kept by the new "mirror.keep" pass (`render::Mirror`). Nothing changes for a frame that keeps no scene. Fall Line keeps it, with `r_reflections 0` to turn it off.
