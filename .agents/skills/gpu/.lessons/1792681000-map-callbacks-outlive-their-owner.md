---
title: A MapAsync callback can run after its owner is destroyed; capture shared state, never this
date: 2026-10-21
scope: engine/gpu/timer.cpp
concepts: [map-async, callback, lifetime, use-after-free, asan, teardown, timestamp-query]
---

Destroying the device completes every map still in flight, so a callback runs during the
device's teardown, after any object declared after the device (a frame, its recorder, its
timer) is gone. `PassTimer`'s callback captured `this` and wrote into a freed timer; the
`taa` lab hit it under the debug row's ASan because it never waited for its last readings.
The callback now captures a `shared_ptr` to the readings and a handle to the buffer. Any new
async callback does the same. Run `make labs-smoke` (or ctest's `labs_smoke`) on the DEBUG
preset before pushing a new GPU lab: `ctest -E labs_smoke` skips exactly that path.
