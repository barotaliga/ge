---
type: Added
title: The live capture: the window and its sound recorded while the game is played
---

`engine/capture/live.hpp` records what the player sees and hears as it happens, into the same WebM (AV1 and Opus) or Matroska file as an export, with no frame waiting for it: the mix is tapped on the audio thread into a lock-free ring whose sample count is the clock, each presented frame is stamped on it and copied back through a ring of readbacks (`engine/gpu/readback_ring.hpp`, `Surface::frame_target`), and a worker thread encodes and writes both tracks in time order. A frame the ring or the worker cannot take is dropped and counted. Fall Line's `--capture=PATH.webm` uses it. In the `gpu-capture-live` lab, 30 s at 60 fps on the software adapter kept all 1800 frames, cost each frame at most 0.9 ms and put every flash within 2 ms of its click.
