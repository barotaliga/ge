---
title: An unbalanced bracket in a WGSL comment silently cuts the shader at that line
date: 2026-10-28
scope: cmake/Wgsl.cmake, engine/render/flame.wgsl
concepts: [wgsl, include, cmake, bracket, entry-point, shaders-check]
---

The include resolver reads a shader as a CMake list, and CMake treats an unbalanced `[` as the
start of a bracket that swallows every line after it. A comment saying "in [0, 1)" in flame.wgsl
dropped the rest of the file, `fs_main` included, and the only symptom was Dawn's "Entry point
fs_main doesn't exist" at pipeline creation (`build/debug/gen/render/<name>.wgsl` shows the cut).
Write ranges as "0..1" in shader comments, and when an entry point goes missing, look at the
resolved file's tail first.
