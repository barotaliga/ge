---
title: A table of times is measured, or the digest differs on every row
date: 2026-10-22
scope: engine/lab/lab.hpp
concepts: [table, digest, measured, bench, smoke, determinism]
---

`Lab::table` hashes every cell into the digest by default, so a table of GPU or CPU times made
the `profile` lab's digest differ between the release and the debug build (and would between
any two machines). Pass `Det::measured` for such a table: it is shown in the report and left
out of the digest, as a measured metric is. Keep anything the no-adapter path cannot produce
(the pass count read off a drawn frame) measured too, so the digest matches on a row without a
GPU.
