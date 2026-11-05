---
type: Fixed
title: Shooter's Edge's map loads no longer stall the window
---

A map's world is now made in small steps, one a frame, behind the spinner, so a load's longest frame is about 50 ms where it was about 0.2 s. The sky no longer decodes the other worlds' moon maps again for every map, and a GPU device keeps each shader it has parsed (`create_shader`), so the next map's passes are not parsed again.
