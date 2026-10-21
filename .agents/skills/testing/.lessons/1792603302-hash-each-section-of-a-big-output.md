---
title: For a large output, golden one hash per section plus the run's log
date: 2026-10-21
scope: porting/parity/golden-mapgen.mjs
concepts: [golden, fnv, section-hash, generator, debug-log]
---

A generated map is millions of numbers, too big for a golden table, and the level's digest
(`level_digest`) only says THAT something moved. The mountain's golden holds one FNV-1a 64 per
SECTION of the level (each grid's raw float32 bytes, the stations, the gates, the trees, the
kickers, the world numbers) plus the debug lines the search logged for the attempts it
refused, hex-encoded. The C++ test lays the same bytes out and names the first section that
differs, which points at the generate step that diverged; the log pins the analysis' message
text (`to_fixed` and `to_string` included) on seeds whose first attempts are refused. Keep the
byte layout written down in both the generator's and the test's headers, and write an absent
optional as the quiet NaN 0x7ff8000000000000.
