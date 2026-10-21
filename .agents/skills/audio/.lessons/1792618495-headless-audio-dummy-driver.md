---
title: The container has no sound card; open the device on SDL's dummy driver
date: 2026-10-21
scope: engine/platform/audio.cpp, tests/platform_audio_test.cpp
concepts: [sdl, audio-device, headless, ci]
---

`open_audio` fails in the cloud container and on CI with no driver. Set
`SDL_AUDIO_DRIVER=dummy` (the test does it with `setenv` before opening): the dummy
device still runs the callback on its own thread at the device's pace, so
`frames_pulled()` climbs and the pull path is tested end to end. A sound is never judged
through the device: render offline and meter it.
