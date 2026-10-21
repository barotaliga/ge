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

## What is built (M6)

- `engine/platform/window.hpp`: `open_window`, `Window::native()` (X11, Wayland, a
  `CAMetalLayer` on Apple, Win32), `pixel_size`, `poll_input` into an `InputSnapshot`
  (keys by USB HID usage, the pointer, focus, resize, quit), `now_ns`, `display_available`.
- `cmake/SDL.cmake`: `GE_PLATFORM` and THE PIN (SDL's version and its archive's SHA-256).
  On Linux SDL needs the X11 and Wayland headers (CI's `GE_LINUX_PACKAGES` lists them); a
  missing one fails the configure with the package to install.
- `games/fall_line/app/main.cpp`: the frame loop: input, focus pauses the run clock, the
  steps it grants, acquire, draw, present.

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

M6 lands `frame-pace`, a `bench` lab over the real window's frame loop beside the `run-clock`
cadence lab. The labs run headless, so nothing in `engine/platform` may be required to run
the `labs` executable.
