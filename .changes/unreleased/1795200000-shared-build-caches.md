---
type: Fixed
title: A new cloud container's first build reuses the shared compiler cache and deps
---

The shared compiler cache never reached a second container: the shared project folder drops any
file over about 500 MB, and the cache was one 544 MB archive. `tools/ccache-share.sh` now saves it
in 200 MB parts behind a small manifest written last, checks every part before a restore, and
carries `.cache/deps/` (Dawn, SDL3 and the codecs built at configure time) the same way. In a
cloud container without ccache, `make build` first runs `tools/setup-ubuntu.sh`, so the first
build compiles through the cache, and `make build` and `make test` save the caches back after a
good build. ccache runs with `base_dir` set to the checkout and `compiler_check=content`, so a
cache from another container or checkout path hits. AGENTS.md says that a project thread started
above the checkout never fires the session-start hook and runs `tools/session-start.sh` itself.
