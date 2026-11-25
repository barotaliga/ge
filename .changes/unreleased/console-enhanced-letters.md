---
type: Fixed
title: The console in classic enhance's letters keeps its size
---

An upscaled glyph sheet (`ui::sheet_font`, cells over 8 px) is drawn at whole multiples of 8 px, filtered from its finer texels, so Shooter's Edge's console in classic enhance's upscaled letters (`ce_pictures`) is the size of the pack's own, not twice it. `find_cache` also finds a cache that holds only the pictures.
