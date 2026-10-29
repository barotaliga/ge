---
title: A C header for the mod ABI still uses // comments, for its ge tags
date: 2026-10-29
scope: engine/mod/abi.hpp, games/shooters_edge/sim/mod_api.hpp
concepts: [abi, c99, ge-tags, agents-check, comments]
---

The ABI headers are plain C so a mod can be written in C, and a block comment looked like the
natural choice. The `ge:` tag reader (`tools/agents-index.mjs`) reads only `//` and `#` lines, so
`make agents-check` called the header untagged. C99 has `//` comments: use them, in the leading
comment and everywhere else, in every header a mod includes.
