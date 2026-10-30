---
title: With every compile cached, a rebuild is the link; measure both before adding more cache
date: 2026-10-30
scope: cmake/Linker.cmake
concepts: [ccache, compiler-cache, lld, linker, cloud-session, build-time]
---

A fresh debug build in the cloud container took 18 minutes; with ccache full it took 2 min 10 s,
and every one of its 1694 compiles was a hit. What was left was the 271 calls ccache cannot
cache, nearly all of them links of the tree's test executables: LLD took that rebuild to 1 min
41 s. The container's own disk is gone with it, so the cache only paid off once it outlived the
container (tools/ccache-share.sh, a 544 MB tar that restores in 2 s). `ccache --zero-stats`
before a build makes `--show-stats` that build's hits; without it the numbers are the cache's
whole life, which is how CI's 93% looked healthy while saying nothing about one run.
