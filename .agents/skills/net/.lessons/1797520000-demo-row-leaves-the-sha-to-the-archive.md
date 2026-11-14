---
title: demo_row leaves the demo's SHA-256 to the archive; set it yourself when you read a file alone
date: 2026-11-14
scope: engine/hub/archive.cpp
concepts: [archive, demo-row, sha256, closing-summary, matches-link-demo]
---

`hub::demo_row(bytes, row, error)` fills a row from the header and trailer but leaves `row.sha`
empty: `archive_add` names the file and sets it. A lab or test that reads a replay file without
the archive must set `row.sha = mod::sha256_hex(...)` after `demo_row` (it resets the row), or a
join by that name links an empty string. `matches_link_demo` now refuses a name that is not 64
characters, so the mistake fails instead of linking nothing.
