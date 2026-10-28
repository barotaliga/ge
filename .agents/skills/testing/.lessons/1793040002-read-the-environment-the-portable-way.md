---
title: Read an environment variable in a test the portable way, or MSVC fails the build
date: 2026-10-24
scope: tests/mountain_village_test.cpp
concepts: [test, golden, getenv, msvc, windows, warning, env]
---

MSVC treats `std::getenv` as deprecated (C4996), and the tree builds with warnings as errors,
so a test that reads `GE_GOLDEN_WRITE` through `std::getenv` builds on Linux and in precheck and
then fails the merge queue's Windows rows. Copy the `env_set` helper the other golden tests use
(`_dupenv_s` under `_MSC_VER`, `std::getenv` elsewhere).
