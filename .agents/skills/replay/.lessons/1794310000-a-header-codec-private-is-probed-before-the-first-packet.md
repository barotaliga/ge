---
title: An encoder that names its parameter sets only with a packet is probed at open
date: 2026-10-30
scope: engine/capture/videotoolbox.cpp, engine/capture/video.hpp
concepts: [videotoolbox, avcc, hvcc, codec-private, probe, header, mp4, matroska, b-frames, interleave]
---

Both muxers write the track's codec private in the file's header, at open, but VideoToolbox
hands its `avcC` or `hvcC` record only with the first keyframe's sample buffer (the format
description's sample description atoms). `PlatformEncoder::open` therefore encodes one black
picture on a throwaway session of the same settings, keeps its record, and fails a later
keyframe whose record differs, rather than deferring every muxer's open. Ask the session for no
B-frames (`AllowFrameReordering` false) and wait for each picture's packet
(`CompleteFrames`): then decode order is presentation order, neither container needs a
composition offset, and the interleave's "oldest picture not yet out" floor holds. Code that
only builds on Apple is checked by the macOS CI row alone, so a draft cannot test it: write the
Linux-checkable half (the words, NV12, the fallback) so precheck holds it, and mark the PR ready
to run the rest.
