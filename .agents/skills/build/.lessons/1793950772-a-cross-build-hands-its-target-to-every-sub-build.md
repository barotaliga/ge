---
title: A dependency built at configure time must be handed the cross target, or it builds for the host under the target's name
date: 2026-11-06
scope: cmake/Codecs.cmake
concepts: [cross-compile, ios, codecs, aom, opus, execute-process, find-root-path, lc-build-version]
---

`ge_codec_build` ran libaom's configure with only the compiler, so the first iOS configure made a
macOS libaom (LC_BUILD_VERSION platform 1) in a folder named for iOS, and find_library then missed
it anyway: a cross build re-roots searches under the SDK unless told `NO_CMAKE_FIND_ROOT_PATH`.
Hand a sub-build CMAKE_SYSTEM_NAME, CMAKE_OSX_SYSROOT, CMAKE_OSX_ARCHITECTURES,
CMAKE_OSX_DEPLOYMENT_TARGET and CMAKE_TOOLCHAIN_FILE (and Android's ABI and platform), keep a copy
per target (the simulator apart from the device), and check what came out with `otool -l <lib> |
grep -A4 LC_BUILD_VERSION` (2 iOS, 7 the simulator).
