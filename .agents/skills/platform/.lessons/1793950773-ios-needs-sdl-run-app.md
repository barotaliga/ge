---
title: On iOS a window opens only inside SDL's UIKit start; the iOS Simulator has no comparison samplers
date: 2026-11-06
scope: engine/platform/window.cpp
concepts: [ios, uikit, sdl-main, run-app, simulator, comparison-sampler, simctl]
---

A plain `main` on iOS fails SDL_Init with "Application didn't initialize properly, did you
include SDL_main.h". engine/platform keeps SDL to itself, so `run_app` calls `SDL_RunApp` (with
SDL_MAIN_HANDLED: the file defines no main) and a game's main hands it its body. To try a build:
`xcrun simctl boot <udid>`, `xcrun simctl install <udid> <app>`, `xcrun simctl launch
--console-pty <udid> <bundle id>` for its stdout, `xcrun simctl io <udid> screenshot <png>`. In
the simulator Dawn's Metal backend refuses comparison samplers, so a frame with shadows loses its
device at once; judge the picture on a device.
