---
type: Added
title: The classic pack's pictures in the cache
---
The classicenhance pipeline gains a `pictures` stage: the pack's status bar pictures, font sheet and menu pictures, read through the engine's new wad and palette picture reader (`vfs::read_wad`, `vfs::read_picture`), are written to the local cache in the palette's colours for a HUD to draw.
