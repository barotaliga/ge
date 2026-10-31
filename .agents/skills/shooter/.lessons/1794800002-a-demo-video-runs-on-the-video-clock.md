---
title: A demo turned into video runs on the video's clock, and sounds play on their frame's sample
date: 2026-10-31
scope: games/shooters_edge/client/demo_video.cpp, games/shooters_edge/client/demo_sound.cpp, engine/capture/av_export.hpp
concepts: [demo-video, export, frame-clock, sound, voice, levels, synth]
---

A demo's own clock starts over at each level and can stall; the video's clock must not. Move it
on by each demo frame's step (clamped, zero at a level's start) and play that frame's sounds and
voices after `AvExport::sound_to(sample of the video clock)`, so a click and its flash stay
within a frame offline. Keep every recording loaded for the export's whole life: the synth reads a
`Pcm` in place. A client recording's model list grows through its signon, so the world's frame is
made again when the list's length changes. Check voices by rendering them alone (`se-demo-video
--game-volume=0`) and reading the level a second; the mix's mean is the game's ambience.
