---
title: A single-header dependency downloads from its pinned commit's raw file, because the container's proxy refuses GitHub's tag archives
date: 2026-10-22
scope: cmake/Cgltf.cmake
concepts: [dependencies, download, proxy, pin]
---

In the cloud container, `https://github.com/<owner>/<repo>/archive/refs/tags/<tag>.tar.gz` (and
codeload's) answers 403 through the agent proxy, while release assets
(`/releases/download/...`, how SDL3 and Dawn come) and `raw.githubusercontent.com/<owner>/<repo>/<commit>/<file>`
download fine. cgltf is one header, so `cmake/Cgltf.cmake` fetches it with `file(DOWNLOAD)` from
the raw URL at the tag's commit, checked against its SHA-256, into `GE_DEPS_DIR`. A multi-file
dependency with no release asset needs `FetchContent` with `GIT_REPOSITORY` and `GIT_TAG` (git
over HTTPS works) instead of a tag archive.
