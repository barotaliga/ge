---
title: A file cut short keeps its whole fragments; a player plays further, and a cut at half proves nothing on a short file
date: 2026-10-29
scope: engine/capture/mp4.cpp, engine/capture/mp4_read.cpp, engine/lab/labs/capture_lab.cpp
concepts: [mp4, fmp4, fragment, crash-safe, truncated, ffmpeg, lab, smoke]
---

The engine's MP4 reader keeps only the fragments whose `moof` and `mdat` are both whole, while
ffmpeg also plays the samples of a cut fragment whose bytes made it to the disk (384 pictures
against the reader's 336 at 24 fps). Hold a cut file to "a player gets at least the whole
fragments", never to an equal count. Cut the file inside its LAST fragment, not at half its
bytes: at the smoke flags (4 s, a fragment every 2 s, the moov and the first keyframe up front)
half the bytes fall inside the first fragment and nothing is whole, which is a true answer that
fails the lab. A sample's duration is the time to the next one on its track, so the muxer holds
each track's newest sample until the next arrives; a fragment cut on a video key takes every video
sample before the key and all but the newest sound packet.
