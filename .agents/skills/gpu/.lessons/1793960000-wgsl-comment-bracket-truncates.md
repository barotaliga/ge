---
title: An unbalanced [ in a WGSL comment cuts the embedded shader short with no error of its own
date: 2026-10-30
scope: cmake/WgslEmbed.cmake, engine/render/*.wgsl
concepts: [wgsl, embed, cmake-list, bracket, truncated-shader, entry-point, shaders-check]
---

The embedding reads a shader through CMake lists, and a `[` with no `]` (a comment saying
"numbers in [0, 1)") stops the list from splitting, so everything after it is dropped. The only
symptom is downstream: a pipeline fails with "Entry point vs_main doesn't exist", and
`make shaders-check` says the file "has no @vertex, @fragment or @compute entry point". Look at
`build/<preset>/gen/<path>.wgsl`'s last lines to find where it was cut; write ranges in words.
