---
title: The ge tag block must end within a file's first 60 lines; a longer header hides it
date: 2026-10-28
scope: tools/agents-index.mjs, games/fall_line/app/frame.hpp
concepts: [ge-tags, agents-check, header, leading-comment, scan-lines]
---

`make agents-check` reads only the first 60 lines of a file (`SCAN_LINES` in
`tools/agents-index.mjs`) for its `ge:` tags. Adding one row to `frame.hpp`'s pass-table comment
pushed `ge:skill` to line 61, and the check reported "missing ge:skill" although the tag was
there. When a header comment grows near 60 lines, tighten a paragraph above the tag block rather
than moving the tags or splitting the comment.
