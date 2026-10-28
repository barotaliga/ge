---
type: Added
title: Recordings compressed as AV1 and Opus, with the engine's own encoders
---

`ge record --format=mkv --codec=av1` writes a run's export as AV1 video and Opus sound in a `.webm` that browsers and players play, with no ffmpeg: about 136 times smaller than the uncompressed file at 44 dB, the sound still on its steps. `--crf` sets the quality (32 by default). The encoders (libaom and libopus, pinned and built once at configure time by `cmake/Codecs.cmake`) are on for this tree's own build and off for the published package (`GE_ENCODERS`); `engine/capture/codecs.hpp` wraps them, and the Matroska muxer writes CodecDelay and SeekPreRoll.
