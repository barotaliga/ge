---
type: Added
title: Demos turned into videos, and demos with voice
---

Shooter's Edge turns any demo into a video with no window (`-demovideo <demo> <file>`): the world drawn offscreen from the recorded eye with the demo's entities, doors and HUD, its own sounds from the packs and its players' voices, written as MP4, WebM or Matroska by `engine/capture`, whose export now also takes a sound the caller plays (`AvExportOptions::synth`, `sound_to`). Our classic server relays voice by team and our client and server record it: a client's `.qwdv` holds its team's voices and its own, the server's `.mvdv` everyone's, each the plain format behind a voice header. The benchmark's console command and flag are now `timedemo` and `-timedemo`. The classic codec's `spawnbaseline2` now carries the entity's number in its record's word as the protocol does, which changes the bytes our server sends.
