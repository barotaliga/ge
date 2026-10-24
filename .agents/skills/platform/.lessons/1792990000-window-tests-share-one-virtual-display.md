---
title: A window test shares its virtual display, so the pointer and the focus are not its own
date: 2026-10-23
scope: tests/platform_devices_test.cpp
concepts: [xvfb, relative-mouse, focus, raw-mouse, ctest-parallel, merge-queue]
---

The raw-mouse case passed in the debug row and locally, but failed on both Linux release rows of
the merge queue. Its motion was not the sum of its pushed deltas, and it was not flagged raw. CI
runs the window tests side by side on one virtual X display, so the window may never get the
focus that relative mode waits for, and another test moving the shared pointer adds motion of its
own. A window test therefore never asserts on frame totals or on state the OS grants. It looks
for its own events in the queue, and asks the platform (`relative_mouse`) whether the mode is in
force before it checks the raw flag. Running six copies at once on one Xvfb locally did not
reproduce the failure, so timing on the runner matters too.
