---
type: Added
title: GE_GPU_LABELS=1 names every pass in a native GPU tool
---

With `GE_GPU_LABELS=1` in the environment, the engine hands its passes' and objects' labels down to the GPU backend, so Instruments' Metal System Trace, Xcode's frame capture, PIX and RenderDoc name each pass as `gpustats` does. Off by default.
