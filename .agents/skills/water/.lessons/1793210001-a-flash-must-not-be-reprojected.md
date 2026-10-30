---
title: A flash must not be reprojected into the frames after it
date: 2026-10-28
scope: engine/render/frame.cpp, engine/render/clouds.wgsl, engine/render/bolts.wgsl
concepts: [lightning, flash, clouds, spread, reprojection, taa, reactive]
---

The cloud is marched a quarter of its pixels a frame, the rest reprojected from the last frame.
A lightning flash lasts a frame or two, so a reprojected pixel would carry the lit cloud into
three dark frames after it, and the flicker would smear into a fade. `fill_uniforms` marches the
quarter whole on a frame with a lamp and the four after it (`FrameHistory::flash_frame`). The
channel pass marks the motion target reactive for the same reason, so the resolve does not keep
a stroke in its history. Anything drawn for one frame needs the same care wherever history is
kept.
