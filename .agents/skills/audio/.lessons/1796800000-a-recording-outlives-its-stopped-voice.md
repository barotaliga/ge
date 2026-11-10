---
title: A recording a stopped voice read is still read for its fade; free it only when Synth::reads is false
date: 2026-11-09
scope: engine/audio/synth.hpp, games/shooters_edge/client/demo_sound.hpp, games/shooters_edge/client/demo_heard.cpp
concepts: [pcm, sample-stop, use-after-free, audio-thread, retired-sounds, escape, playdemo]
---

`sample_stop` only sends a command: the render thread applies it at its next block and fades the
voice for 7.5 time constants (75 ms at the default 0.01 s), reading the `Pcm` in place the whole
time. Freeing the recording right after the stop (DemoHearing::stop freed its DemoSounds at
Escape) is a use-after-free on the audio thread: a segfault on a Mac, invisible in tests and in a
container with no audio device. Keep it until `Synth::reads(pcm)` is false (`RetiredSounds` does
this for a DemoSounds), and declare whatever owns recordings before the audio device so the device
closes first on the way out. To catch it in the container, run the window with
`SDL_AUDIO_DRIVER=dummy` (its thread pulls the synth) under ASan; a test can show it single-threaded:
play, stop, free, then `render`.
