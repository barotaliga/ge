---
title: Pick the codec whose library builds with CMake everywhere
date: 2026-10-24
scope: cmake/Codecs.cmake, engine/capture/codecs.cpp
concepts: [encoder, av1, vp9, libaom, libvpx, build, windows, nasm, deps]
---

libvpx (VP9) builds only with its own configure script and make, an msys shell on Windows; libaom
(AV1) and libopus build with CMake on all five CI rows, so the export encodes AV1. Build such a
library once at configure time into `GE_DEPS_DIR`, keyed by platform and compiler, and link it
`PRIVATE $<BUILD_INTERFACE:...>` behind an option that the packaging workflows turn off, or the
installed `ge_play` names a library the package does not ship. libaom's x86 assembly needs nasm;
without it pass `AOM_TARGET_CPU=generic` rather than failing the build.
