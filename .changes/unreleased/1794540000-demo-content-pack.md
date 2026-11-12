---
type: Added
title: The hub sends a demo only the files it needs
---

The hub makes each archived demo its own content pack: the maps, models and sounds the demo draws
from, taken out of the packs beside the web viewer, zstd-packed and named by its hash, made when
the demo arrives. The Demo page hands the viewer that one pack, so a 20-minute 4on4 downloads
1.6 MB of content instead of 52 MB of packs.
