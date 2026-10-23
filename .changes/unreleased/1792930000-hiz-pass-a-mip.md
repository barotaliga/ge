---
type: Fixed
title: The Hi-Z pyramid is built a pass a mip
---

`render::encode_hiz` reduces each mip of the depth pyramid in its own compute pass, so no mip is read before the one below it is written on any backend (a high mip holding the last frame's depth lets the late cull drop what shows). `FrameRecorder::compute_part` times and counts such a chain as one pass.
