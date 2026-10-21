---
title: A lab registers from a static initializer, so it must be compiled into the labs executable, never a library
date: 2026-10-21
scope: CMakeLists.txt, engine/lab/lab.hpp
concepts: [registration, cmake, static-library, linker, glob]
---

`GE_LAB(spec)` registers a lab from a static object's constructor. The linker drops object
files from a static library unless something references them, and nothing references a lab
file, so a lab compiled into a library vanishes from `labs` with no error. CMake globs every
`.cpp` in `engine/lab/labs/`, `plugins/*/labs/` and `games/*/labs/` straight into the `labs`
executable for that reason. A helper those labs share goes in the same `labs/` directory.
Do not move lab files into a library. If a lab is missing from `make labs`, check that its file
sits in one of those directories, then re-run `cmake --preset` so the glob sees the new file.
