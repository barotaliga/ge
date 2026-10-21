---
title: Under ASan, lavapipe's worker threads leak at exit, and Xvfb has no vsync
date: 2026-10-21
scope: tests/CMakeLists.txt
concepts: [lavapipe, asan, leak-sanitizer, xvfb, vsync, window-test]
---

Every test that makes a device on lavapipe ends with LeakSanitizer reporting a few
hundred bytes of "indirect" leaks from the driver's threads (no frame of ours in them), and
LeakSanitizer's exit swallows the test's stdout. `gpu_` and `platform_` tests therefore
run with `ASAN_OPTIONS=detect_leaks=0` (tests/CMakeLists.txt); every other test keeps the
check. Under `xvfb-run` a FIFO surface presents unthrottled (0.7 ms a frame at 320x180), so
the window test proves the surface path, not frame pacing.
