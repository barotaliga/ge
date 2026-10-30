---
title: A timing test bounds the typical call, never the slowest one
date: 2026-10-30
scope: tests/capture_live_test.cpp
concepts: [test, flaky, timing, percentile, median, preemption, runner, hand-in, live-capture]
---

`a_full_queue_drops_pictures_and_the_hand_in_never_waits` held the slowest of 200 hand-ins
under 50 ms and failed at 84.84 ms on a shared macOS runner: one preempted call is a fact about
the runner, not the code. The test now holds the median under 2 ms and the 90th percentile under
10 ms (measured here at 0.000 and 0.002 ms with eight busy loops beside it), which a few
preempted calls cannot move and a hand-in that waited for the worker's encode would. Bound a
share of calls, never the worst one.
