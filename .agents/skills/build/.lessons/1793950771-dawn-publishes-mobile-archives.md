---
title: Dawn's release carries mobile archives; read the release's assets before building a dependency from source
date: 2026-11-06
scope: cmake/Dawn.cmake
concepts: [dawn, ios, android, xcframework, prebuilt, cross-compile, release-assets]
---

The roadmap said iOS and Android would build Dawn from source. The pinned release already held
`dawn-apple-<commit>.xcframework.tar.gz` (iOS device, iOS Simulator, Mac), `dawn-android-<commit>
.tar.gz` (four ABIs) and `dawn-headers-<commit>.tar.gz`: `gh release view <tag> -R google/dawn
--json assets` lists them. The headers archive's `lib/cmake/Dawn` was written by the Android
build (it names that runner's `liblog.so` by absolute path), so declare the imported target
yourself over the slice and the system frameworks.
