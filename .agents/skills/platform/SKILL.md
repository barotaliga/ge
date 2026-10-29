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
- **Input**: the window's own `InputSnapshot` (keys, pointer, focus, quit), and every input
  device as engine/input events through `devices.hpp` (keyboards, raw mice, gamepads,
  joysticks, wheels, touch; rumble, lights and force feedback back out) read in the same pump.
  The device side is the `input` skill's (`docs/input.md` § The platform backend).
- **Focus**: losing focus pauses the run clock; regaining it resumes with an empty
  accumulator (`engine/loop/run_clock.hpp`'s rule).
- **Audio device**: one output stream at the device's rate. The `audio` layer fills it.
- **Files**: the read-only asset path and a writable save directory, one file per stored
  key (settings, records, ghosts).
- **Time**: a monotonic clock for the frame delta only. The simulation never sees it.

## What is built (M6)

- `engine/platform/window.hpp`: `open_window`, `Window::native()` (X11, Wayland, a
  `CAMetalLayer` on Apple, Win32), `pixel_size`, `poll_input` into an `InputSnapshot`
  (keys by USB HID usage, the pointer, focus, resize, quit), `now_ns`, `display_available`.
- `cmake/SDL.cmake`: `GE_PLATFORM` and THE PIN (SDL's version and its archive's SHA-256).
  On Linux SDL needs the X11 and Wayland headers (`tools/setup-ubuntu.sh` installs them); a
  missing one fails the configure with the package to install.
- `games/fall_line/app/main.cpp`: the frame loop: input, focus pauses the run clock, the
  steps it grants, acquire, draw, present.

## What is built (M9a)

- `engine/platform/audio.hpp`: `open_audio` opens the default output as a 48 kHz stereo float
  stream and pulls it from SDL's audio thread (`AudioDesc::pull`, the synth's `render`); SDL
  converts to the hardware's rate. `frames_pulled`, `driver`, `pause_audio`, `resume_audio`,
  `audio_available`.
- Headless, SDL's `dummy` driver (`SDL_AUDIO_DRIVER=dummy`) consumes the stream at the device's
  rate with no hardware: `tests/platform_audio_test.cpp` runs the real device path that way.
- On Linux SDL finds PipeWire, PulseAudio and ALSA by their headers (`libpipewire-0.3-dev`,
  `libpulse-dev`, `libasound2-dev`, in `tools/setup-ubuntu.sh`); without them it builds with
  the dummy and disk drivers only, and a desktop build is silent.

## Rules

- SDL is fetched with `FetchContent`, pinned to a release tag, and statically linked.
- Poll events once per frame on the main thread.
- The mobile builds (M11: iOS, iPadOS, Android) use SDL's iOS and Android backends through the same API. There is no web build.
- Headless tests never create a window. A test that needs one checks
  `display_available()` and skips without a display; in the container and on CI's Linux
  x64 rows it runs under `xvfb-run -a`.
- A new window system (Android, UIKit in M11) is a `NativeWindow::Kind`, a case in
  `Window::native` and a case in `engine/gpu/surface.cpp`, nothing else.

## Labs

- **`frame-pace`** (`engine/lab/labs/platform_frame_pace_lab.cpp`, a `bench`): the real
  window's loop presents the test scene N frames and reports the present intervals (mean,
  p50, p99, max) and the run clock's steps a frame, all `measured`. Run it as
  `xvfb-run -a build/debug/labs frame-pace` in the container, where Xvfb has no vsync and
  the intervals are the loop's own cost; pacing at vsync is a real-display check. Without a
  display it notes that and passes with the same digest, so `labs_smoke` never needs one.
