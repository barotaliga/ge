---
title: A page that keeps working after it loads is shot over Chromium's DevTools pipe, after a wall-clock wait
date: 2026-11-12
scope: engine/lab/labs/hub_browser.hpp
concepts: [chromium, headless, screenshot, devtools, cdp, webgpu, wasm, hub-viewer]
---

Chromium's `--screenshot` takes its picture at the load event or at `--timeout`. `--timeout`
cancels any fetch still running, such as the viewer's 6.5 MB wasm. `--virtual-time-budget` runs
the page's clock ahead of the GPU, so the canvas is still empty when the picture is taken. For a
page that draws on WebGPU, use `hub_browser::live_draw` instead:

- It starts the browser with `--remote-debugging-pipe` (fds 3 and 4, NUL-delimited JSON).
- It opens the page with `Target.createTarget` and `Target.attachToTarget`.
- It waits real time, then asks for `Page.captureScreenshot`.

SwiftShader needs about 20 s to draw the viewer's first frame, and the screenshot itself can take
a minute. Give the wait 25 s and the screenshot two minutes.
