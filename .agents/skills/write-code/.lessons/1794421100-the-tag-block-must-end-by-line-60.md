---
title: A file's ge: tags are read from its first 60 lines only
date: 2026-11-11
scope: tools/agents-index.mjs, games/shooters_edge/server/sequel_server.hpp
concepts: [ge-tags, leading-comment, agents-check, scan-lines, missing-ge-skill]
---

`tools/agents-index.mjs` reads only a file's first 60 lines (`SCAN_LINES`) for its tag block, so
a header comment that grows past that loses its last tags: agents-check then says "missing
ge:skill" (or `ge:test`) although the tag is in the file. Shorten the leading comment (move the
detail beside the code it describes) rather than reorder the tags.
