---
title: A GPU test returns when test_device gives no device, even inside a helper
date: 2026-10-24
scope: tests/render_ui_pass_test.cpp, tests/gpu_support.hpp
concepts: [gpu, skip, test_device, windows, merge-queue]
---

`ge::test::test_device()` only marks the test as skipped when no device can be made; it does
not stop the test, so it returns null and the caller must return. A helper that builds a rig
around it (`make_rig`) has to return null too, and every test that calls the helper has to
return when it gets null. Otherwise the test dereferences the null device and crashes. The
pull-request row never shows this, because Linux always has lavapipe. The merge queue's Windows
row does show it: there Dawn sometimes cannot load its DLLs, every other GPU test skips, and
the one without the guard crashes and gets the PR ejected.
