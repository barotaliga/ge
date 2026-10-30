---
type: Added
title: The platform's video encoders: VideoToolbox's H.264 and HEVC on a Mac, AV1 behind them
---

`ge record --format=mkv --codec=h264` (or `hevc`) codes the pictures with the encoder the operating system ships, through the machine's media engine where it has one, into the engine's own `.mp4` (or a Matroska file with `--container=mkv`), with the sound in Opus. `--codec=auto` takes the platform's H.264 where there is one and the engine's own AV1 elsewhere, and `fall_line --capture=run.mp4` captures with it, so on a Mac the window is coded by the media engine. VideoToolbox (`engine/capture/videotoolbox.cpp`) is the first platform; Media Foundation, MediaCodec and VA-API fill the same seam later. A `.webm` now refuses tracks WebM cannot carry. The `capture` lab gains a platform table (which encoder coded, the sync fixture as an `.mp4` decoded back, a 720p frame's cost) and the `sync-platform` shot.
