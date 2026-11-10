---
title: Apple Clang lets an int times a float through that GCC's -Wconversion rejects; check a skip-ci PR with GCC on the Mac
date: 2026-11-05
scope: tests/render_morph_test.cpp, .github/workflows/ci.yml, tools/precheck.sh
concepts: [gcc, clang, wconversion, int-to-float, skip-ci, merge-queue, linux-gcc, homebrew, isysroot]
---

`0.004f * i` with `i` an `int` passes Apple Clang's `-Wconversion` (and so `make precheck` on the Mac) but GCC rejects it ("conversion from 'int' to 'float' may change value"). A `skip-ci` PR then fails the queue's only job, `linux-gcc`, and is ejected. Write `0.004f * float(i)`.

To check a change with GCC on the Mac before arming: `brew install gcc`, then compile each changed `.cpp` with `g++-16 -isysroot "$(xcrun --show-sdk-path)" -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Werror '-DGE_SOURCE_DIR="."' -Iengine -Iplugins -Igames -Itests -Ibuild/debug/gen -isystem <Dawn's include>`. Without `-isysroot` GCC cannot find the SDK's C headers and every file stops at its first include, which reads as a pass if you only grep for warnings. Pass the define from a script, not through `eval`, or its quotes are lost. A missing `ge` command, `ge check --gcc`, would do this in one step.
