---
title: The tree's libaom is realtime only, so a still is a realtime keyframe
date: 2026-11-02
scope: cmake/Codecs.cmake, engine/capture/codecs.cpp, engine/capture/still.cpp
concepts: [libaom, all-intra, realtime-only, avif, still, screenshot, usage, keyframe]
---

`aom_codec_enc_config_default(..., AOM_USAGE_ALL_INTRA, ...)` fails here: `cmake/Codecs.cmake`
builds libaom with `CONFIG_REALTIME_ONLY=1` (smaller and quicker on every CI row), which leaves out
the all-intra and good-quality modes. An AVIF screenshot (`Av1Options::still`) is therefore one
realtime keyframe at speed 7, realtime's slowest; on a 1440p game frame at quality 90 it is still
about half a 4:4:4 JPEG's bytes at a similar PSNR. Turning the other modes on means rebuilding
libaom on every platform's dependency cache: measure what all-intra buys with the `screenshot`
lab before paying that.
