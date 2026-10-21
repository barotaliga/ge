---
title: For a large output, golden one hash per section plus the oracle's log
date: 2026-10-21
scope: tools/parity/golden-mapgen.mjs
concepts: [golden, fnv, section-hash, generator, debug-log]
---

A generated map is millions of numbers, too big for a golden table, and game5's own digest
(`levelDigest`) only says THAT something moved. `golden-mapgen.mjs` writes one FNV-1a 64 per
SECTION of the level (each grid's raw float32 bytes, the stations, the gates, the trees, the
kickers, the world numbers) plus the debug lines the search logged for the attempts it
refused, hex-encoded. The C++ test lays the same bytes out and names the first section that
differs, which points at the generate step that diverged; the log pins the analysis' message
text (`toFixed` and `String(n)` included) on seeds whose first attempts are refused. Keep the
byte layout written down in both files' headers, and write an absent optional the way JS's
DataView does (`NaN` is 0x7ff8000000000000).
