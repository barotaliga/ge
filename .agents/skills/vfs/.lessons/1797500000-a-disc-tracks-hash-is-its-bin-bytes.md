---
title: A disc's audio track hashed as 16-bit little-endian samples equals its bytes in the .bin
date: 2026-11-15
scope: engine/vfs/disc.hpp, plugins/classic/content/soundtrack.hpp
concepts: [disc, cue, bin, audio-track, sha256, soundtrack, flac]
---

An image keeps an AUDIO track as raw 2352-byte sectors of 44.1 kHz 16-bit stereo, little-endian,
so hashing the decoded samples as little-endian 16-bit gives the same SHA-256 as the track's own
bytes in the `.bin`. The soundtrack store keys tracks that way: a WAV rip, a FLAC rip and the
image agree, and `ffmpeg -i trackNN.flac -f s16le - | sha256sum` checks a kept FLAC against the
manifest's `pcm=` with no engine code. A cue track runs INDEX 01 to the next track's INDEX 01 (the
gap goes with the track before it, as rips keep it); with another convention the hashes differ by
the gap, so record known tracks from images ripped that way. A Joliet volume name is padded with
UCS-2 spaces (00 20), not ASCII ones: a writer that pads with 0x20 bytes reads back as U+2020.
