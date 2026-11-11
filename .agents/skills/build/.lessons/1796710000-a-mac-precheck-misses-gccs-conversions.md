---
title: A precheck on the Mac misses GCC's -Wconversion; scan the changed files with Homebrew's GCC
date: 2026-11-10
scope: tools/precheck.sh, CMakeLists.txt, .github/workflows/ci.yml
concepts: [gcc, wconversion, apple-clang, macos, precheck, merge-queue, size-t-to-double]
---

`make precheck` on macOS builds with Apple Clang, which passes `x / std::max<size_t>(1, n)` into a
double; GCC's `-Wconversion` ("conversion from 'long unsigned int' to 'double' may change value")
with `-Werror` fails the queue's Linux GCC row, and the queue ejects the PR (one round lost). On a
Mac with Homebrew's GCC (`g++-16`), check every changed `.cpp` first with GCC's own warnings:
take each file's `-I`/`-isystem`/`-D` flags from `build/debug/compile_commands.json` and run
`g++-16 --sysroot=$(xcrun --show-sdk-path) -fsyntax-only -std=c++20 -Wall -Wextra -Wpedantic
-Wshadow -Wconversion -Wno-sign-conversion <flags> <file>` (without `--sysroot` every file stops
at `stdint.h`). On a 105-file branch it ran in about a minute and named the one line the queue
had failed on. Write a 64-bit count into a double with `static_cast<double>(...)`.
