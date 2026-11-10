---
type: Fixed
title: A new map appears in Shooter's Edge with no dropped frames
---

A map's world is warmed up offscreen before it is shown (its sky's tables and its lights' shadows drawn a few at a time from where the player will stand), so the first frame of a new map no longer stalls; the title's clouds are drawn at a quarter of the pixels over four frames. A load's longest frame is now about 30 ms, one missed refresh at most. The BSP renderer gains `Options::first_all` and `encode_bsp_shadows`.
