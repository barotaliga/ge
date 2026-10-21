---
name: platform
description: "Use for engine/platform in gameengine2: SDL3 behind a narrow API — the window and surface handles, keyboard/pointer/touch/gamepad input and rumble, the audio device stream, time, storage paths, and focus/pause."
---

# Platform (SDL3)

`engine/platform/` is the only directory that includes SDL headers. Everything above it
sees plain structs.

## What Fall Line needs from it (and nothing more)

- **Window**: create, resize, fullscreen toggle, DPI scale, and the native handles the
  GPU surface needs (`gpu` skill). Vsync is the GPU's job.
- **Input**: keyboard (the game's default keys), pointer and touch (the HUD presses and
  thumb zones), gamepad axes and buttons, and gamepad rumble. Platform input becomes a
  per-frame `InputSnapshot`, and the game maps it to its `SkierInput` (the one place
  screen axes become engine signs).
- **Focus**: losing focus pauses the run clock; regaining it resumes with an empty
  accumulator (`engine/loop/run_clock.hpp`'s rule).
- **Audio device**: one output stream at the device's rate. The `audio` layer fills it.
- **Files**: the read-only asset path and a writable save directory, one file per stored
  key (settings, records, ghosts).
- **Time**: a monotonic clock for the frame delta only. The simulation never sees it.

## Rules

- SDL is fetched with `FetchContent`, pinned to a release tag, and statically linked.
- Poll events once per frame on the main thread.
- The mobile builds (M11: iOS, iPadOS, Android) use SDL's iOS and Android backends through the same API. There is no web build.
- Headless tests never create a window. The platform layer has a null backend for
  CI.
